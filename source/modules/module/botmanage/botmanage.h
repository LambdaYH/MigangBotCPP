#pragma once

#include "modules/module_interface.h"
#include "module_manager.h"

#include "modules/module/botmanage/sv.h"
#include "modules/module/botmanage/feedback.h"
#include "modules/module/botmanage/friend_invite.h"
#include "modules/module/botmanage/group_invite.h"
#include "modules/module/botmanage/help.h"
#include "modules/module/botmanage/status_info.h"
#include "modules/module/botmanage/baned_kicked.h"
#include "modules/module/botmanage/dismiss.h"

namespace white {
namespace module {

class BotManage : public ModulePack {
 public:
  virtual void Register() override {
      SubModule<SV>();
      SubModule<Help>();
      SubModule<StatusInfo>();
      SubModule<FeedBack>();
      SubModule<FriendInvite>();
      SubModule<GroupInvite>();
      SubModule<BandedOrKicked>();
      SubModule<Dismiss>();
  }
};
}  // namespace module
}  // namespace white