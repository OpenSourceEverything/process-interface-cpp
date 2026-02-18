#ifndef PROCESS_INTERFACE_IPC_IMPL_ZMQ_CONTEXT_H
#define PROCESS_INTERFACE_IPC_IMPL_ZMQ_CONTEXT_H

namespace ProcessInterface {
namespace Ipc {

class ZmqContext {
public:
    ZmqContext();
    ~ZmqContext();

    void* raw() const;
    bool valid() const;

private:
    void* context_;
};

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_IMPL_ZMQ_CONTEXT_H
