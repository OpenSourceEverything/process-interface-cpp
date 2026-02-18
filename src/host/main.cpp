#include "../host_runtime/host_runtime.h"

int main(int argc, char** argv) {
    return ProcessInterface::HostRuntime::RunHost(argc, argv);
}
