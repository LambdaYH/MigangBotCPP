#ifndef MIGANGBOTCPP_BOT_BOT_H_
#define MIGANGBOTCPP_BOT_BOT_H_

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <thread>
#include <mutex>
#include <string>
#include <utility>
#include <functional>

#include "bot/api_bot.h"
#include "version.h"
#include "event/event.h"
#include "event/event_handler.h"

namespace white
{

using Json = nlohmann::json;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

inline void fail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

class Bot : public std::enable_shared_from_this<Bot>
{
public:
    explicit 
    Bot(tcp::socket&& socket, std::size_t write_thread_num = 4, std::size_t process_thread_num = 4);
    ~Bot();

    void Run();

    // Start the asynchronous operation
    void OnRun();

    void OnAccept(beast::error_code ec);

    void DoRead();

    void OnRead(beast::error_code ec, std::size_t bytes_transferred);

private:
    void StartThread(std::size_t write_thread_num, std::size_t process_thread_num);

    void ThreadFunctionWrite();

    void ThreadFunctionProcess();

    void Notify(const std::string &msg);

    void SetEchoFunction(const int echo_code, std::function<void(const Json &)> &&func);

    bool EventProcess(const Event &event);

private:
    beast::websocket::stream<boost::beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::condition_variable cond_write_;
    std::condition_variable cond_process_;
    tbb::concurrent_queue<std::string> writable_msg_queue_;
    tbb::concurrent_queue<std::string> processable_msg_queue_;
    tbb::concurrent_unordered_map<int, std::function<void(const Json &)>> echo_function_;
    std::mutex mutex_write_;
    std::mutex mutex_process_;
    std::function<void(const std::string &)> notify_;
    std::function<void(const int, std::function<void(const Json &)> &&)> set_echo_function_;
    onebot11::ApiBot api_bot_;
    std::function<bool(Event &)> event_handler_;

    bool stop_;
    std::vector<std::thread> threads_;
};

inline Bot::Bot(tcp::socket&& socket, std::size_t write_thread_num, std::size_t process_thread_num) :
        ws_(std::move(socket)),
        notify_(std::bind(&Bot::Notify, this, std::placeholders::_1)),
        set_echo_function_(std::bind(&Bot::SetEchoFunction, this, std::placeholders::_1, std::placeholders::_2)),
        api_bot_(notify_, set_echo_function_),
        event_handler_( std::bind(&EventHandler::Handle, &EventHandler::GetInstance(), std::placeholders::_1, std::ref(api_bot_)) ),
        stop_(false)
{
    StartThread(write_thread_num, process_thread_num);
}

inline Bot::~Bot()
{
    stop_ = true;
    cond_process_.notify_all();
    cond_write_.notify_all();
    for(auto &thread : threads_)
        thread.join();
}

inline void Bot::Run()
{
    net::dispatch(boost::asio::make_strand(ws_.get_executor()),
                beast::bind_front_handler(
                    &Bot::OnRun,
                    shared_from_this()));
}

inline void Bot::OnRun()
{
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(http::field::server,
                std::string(MIGANGBOTCPP_VERSION) +
                    " server-async");
        }));
    // Accept the websocket handshake
    ws_.async_accept(
        beast::bind_front_handler(
            &Bot::OnAccept,
            shared_from_this()));
}

inline void Bot::OnAccept(beast::error_code ec)
{
    if(ec)
        return fail(ec, "accept");

    DoRead();
}

inline void Bot::DoRead()
{
    ws_.async_read(
    buffer_,
    beast::bind_front_handler(
        &Bot::OnRead,
        shared_from_this()));
}

inline void Bot::OnRead(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if(ec == websocket::error::closed)
        return;

    if(ec)
        return fail(ec, "read");

    processable_msg_queue_.push(std::move(beast::buffers_to_string(buffer_.data())));
    cond_process_.notify_one();
    buffer_.consume(buffer_.size());
    DoRead();
}


inline void Bot::Notify(const std::string &msg)
{
    writable_msg_queue_.push(msg);
    cond_write_.notify_one();
    LOG_DEBUG("Msg To sent: {}", msg);
}

inline void Bot::SetEchoFunction(const int echo_code, std::function<void(const Json &)> &&func)
{
    echo_function_.emplace(echo_code, std::move(func));
}

inline void Bot::StartThread(std::size_t write_thread_num, std::size_t process_thread_num)
{
    for(std::size_t i = 0; i < process_thread_num; ++i)
        threads_.push_back(std::thread{&Bot::ThreadFunctionProcess, this});
    for(std::size_t i = 0; i < write_thread_num; ++i)
        threads_.push_back(std::thread{&Bot::ThreadFunctionWrite, this});
}

inline void Bot::ThreadFunctionProcess()
{
    Event msg;
    std::unique_lock<std::mutex> locker(mutex_process_);
    std::string msg_str;
    while(!stop_)
    {
        if(processable_msg_queue_.empty())
            cond_process_.wait(locker);
        else
        {
            locker.unlock();
            if(processable_msg_queue_.try_pop(msg_str))
            {
                LOG_DEBUG("Recieve: {}", msg_str);
                try
                {
                    auto msg = nlohmann::json::parse(msg_str);
                    if(EventProcess(msg))
                        event_handler_(msg);
                }catch(nlohmann::json::exception &e)
                {
                    LOG_ERROR("Exception: {}", e.what());
                }
            }
            locker.lock();
        }
    }
}

inline void Bot::ThreadFunctionWrite()
{
    beast::flat_buffer buffer;
    boost::system::error_code ec;
    std::string msg_str;
    std::unique_lock<std::mutex> locker(mutex_write_);
    while(!stop_)
    {
        if(writable_msg_queue_.empty())
            cond_write_.wait(locker);
        else{
            locker.unlock();
            if(writable_msg_queue_.try_pop(msg_str))
            {
                beast::ostream(buffer) << msg_str;
                ws_.text(ws_.got_text());
                ws_.write(buffer.data(), ec);
                if(ec)
                    LOG_ERROR("In ThreadFunctionWrite: {}", ec.message());
                buffer.consume(buffer.size());
            }
            locker.lock();
        }
    }
}

inline bool Bot::EventProcess(const Event &event)
{
    if(event.contains("retcode"))
    {
        if(event.value("status", "failed") == "ok")
        {
            auto echo_code = event.value("echo", 0);
            if(echo_function_.count(echo_code))
                echo_function_.at(echo_code)(event["data"]);
        }
        return false;
    }else if(event.contains("message"))
    {
        QId user_id = event["user_id"].get<QId>();
        if(event.contains("group_id"))
        {
            GId group_id = event["group_id"].get<GId>();
            if(api_bot_.IsNeedMessage(group_id, user_id))
                api_bot_.FeedMessage(group_id, user_id, event["message"].get<std::string>());
        }
        if(api_bot_.IsSomeOneNeedMessage(user_id))
            api_bot_.FeedMessageTo(user_id, event["message"].get<std::string>());
    }
    return true;
}

} // namespace white

#endif