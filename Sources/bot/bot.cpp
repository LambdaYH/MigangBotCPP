#include "bot/bot.h"
#include "version.h"
#include "event/event_handler.h"
#include "logger/logger.h"
#include <iostream>

namespace white
{

void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

Bot::Bot(tcp::socket&& socket, std::size_t write_thread_num, std::size_t process_thread_num) :
        ws_(std::move(socket)),
        notify_(std::bind(&Bot::Notify, this, std::placeholders::_1)),
        set_echo_function_(std::bind(&Bot::SetEchoFunction, this, std::placeholders::_1, std::placeholders::_2)),
        stop_(false),
        api_bot_(notify_, set_echo_function_),
        event_handler_( std::bind(&EventHandler::Handle, &EventHandler::GetInstance(), std::placeholders::_1, std::ref(api_bot_)) )
{
    StartThread(write_thread_num, process_thread_num);
}

Bot::~Bot()
{
    stop_ = true;
    cond_process_.notify_all();
    cond_write_.notify_all();
}

void Bot::Run()
{
    net::dispatch(ws_.get_executor(),
                beast::bind_front_handler(
                    &Bot::OnRun,
                    shared_from_this()));
}

void Bot::OnRun()
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

void Bot::OnAccept(beast::error_code ec)
{
    if(ec)
        return fail(ec, "accept");

    DoRead();
}

void Bot::DoRead()
{
    ws_.async_read(
    buffer_,
    beast::bind_front_handler(
        &Bot::OnRead,
        shared_from_this()));
}

void Bot::OnRead(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if(ec == websocket::error::closed)
        return;

    if(ec)
        return fail(ec, "read");

    {
        std::lock_guard<std::mutex> locker(mutex_process_);
        processable_msg_queue_.push(std::move(beast::buffers_to_string(buffer_.data())));
    }
    cond_process_.notify_one();
    buffer_.consume(buffer_.size());
    DoRead();
}

void Bot::StartThread(std::size_t write_thread_num, std::size_t process_thread_num)
{
    for(std::size_t i = 0; i < process_thread_num; ++i)
        std::thread{&Bot::ThreadFunctionProcess, this}.detach();
    for(std::size_t i = 0; i < write_thread_num; ++i)
        std::thread{&Bot::ThreadFunctionWrite, this}.detach();
}

void Bot::ThreadFunctionProcess()
{
    Event msg;
    std::unique_lock<std::mutex> locker(mutex_process_);
    while(!stop_)
    {
        if(processable_msg_queue_.empty())
            cond_process_.wait(locker);
        else
        {
            auto msg_str = std::move(processable_msg_queue_.front());
            processable_msg_queue_.pop();
            locker.unlock();
            LOG_DEBUG("Recieve: {}", msg_str);
            auto msg = nlohmann::json::parse(msg_str);
            if(EventProcess(msg))
                event_handler_(msg);
            locker.lock();
        }
    }
}

void Bot::ThreadFunctionWrite()
{
    beast::flat_buffer buffer;
    std::unique_lock<std::mutex> locker(mutex_write_);
    while(!stop_)
    {
        if(writable_msg_queue_.empty())
            cond_write_.wait(locker);
        else{
            std::string msg = std::move(writable_msg_queue_.front());
            writable_msg_queue_.pop();
            locker.unlock();
            beast::ostream(buffer) << msg;
            ws_.text(ws_.got_text());
            ws_.write(buffer.data());
            buffer.consume(buffer.size());
            locker.lock();
        }
    }
}

bool Bot::EventProcess(const Event &event)
{
    if(event.contains("retcode"))
    {
        if(event.value("status", "failed") == "ok")
        {
            auto echo_code = event.value("echo", 0);
            if(echo_function_.count(echo_code))
            {
                echo_function_.at(echo_code)(event["data"]);
                echo_function_.erase(echo_code);
            }
        }
        return false;
    }
    return true;
}

} // namespace white