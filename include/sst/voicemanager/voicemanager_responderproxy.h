//
// Created by Paul Walker on 3/3/25.
//

#ifndef VOICEMANAGER_RESPONDERPROXY_H
#define VOICEMANAGER_RESPONDERPROXY_H

namespace sst::voicemanager::details
{
template<typename VM_t>
struct ResponderProxy
{
  VM_t &vm;
  ResponderProxy(VM_t &vm) : vm(vm) {}
};

template<typename VM_t>
struct MonoResponderProxy
{
  VM_t &vm;
  MonoResponderProxy(VM_t &vm) : vm(vm) {}
};
}
#endif //VOICEMANAGER_RESPONDERPROXY_H
