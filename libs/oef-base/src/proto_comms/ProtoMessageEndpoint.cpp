#include "oef-base/proto_comms/ProtoMessageEndpoint.hpp"

#include "oef-base/proto_comms/ProtoMessageReader.hpp"
#include "oef-base/proto_comms/ProtoMessageSender.hpp"
#include "oef-base/proto_comms/ProtoPathMessageReader.hpp"
#include "oef-base/proto_comms/ProtoPathMessageSender.hpp"
#include "oef-base/utils/Uri.hpp"

template <typename TXType, typename Reader, typename Sender>
ProtoMessageEndpoint<TXType, Reader, Sender>::ProtoMessageEndpoint(
    std::shared_ptr<EndpointType> endpoint)
  : EndpointPipe<EndpointBase<TXType>>(std::move(endpoint))
{}

template <typename TXType, typename Reader, typename Sender>
void ProtoMessageEndpoint<TXType, Reader, Sender>::setup(
    std::shared_ptr<ProtoMessageEndpoint> &myself)
{
  std::weak_ptr<ProtoMessageEndpoint> myself_wp = myself;

  protoMessageSender = std::make_shared<Sender>(myself_wp);
  endpoint->writer   = protoMessageSender;

  protoMessageReader = std::make_shared<Reader>(myself_wp);
  endpoint->reader   = protoMessageReader;
}

template <typename TXType, typename Reader, typename Sender>
void ProtoMessageEndpoint<TXType, Reader, Sender>::setEndianness(Endianness newstate)
{
  protoMessageReader->setEndianness(newstate);
  protoMessageSender->setEndianness(newstate);
}

template class ProtoMessageEndpoint<std::shared_ptr<google::protobuf::Message>>;
template class ProtoMessageEndpoint<std::pair<Uri, std::shared_ptr<google::protobuf::Message>>,
                                    ProtoPathMessageReader, ProtoPathMessageSender>;
