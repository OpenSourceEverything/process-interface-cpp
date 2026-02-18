#include "ZmqContext.h"

#include <zmq.h>

namespace ProcessInterface {
namespace Ipc {

ZmqContext::ZmqContext()
    : context_(NULL) {
    context_ = zmq_ctx_new();
}

ZmqContext::~ZmqContext() {
    if (context_ != NULL) {
        zmq_ctx_term(context_);
        context_ = NULL;
    }
}

void* ZmqContext::raw() const {
    return context_;
}

bool ZmqContext::valid() const {
    return context_ != NULL;
}

}  // namespace Ipc
}  // namespace ProcessInterface
