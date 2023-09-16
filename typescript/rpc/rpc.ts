import { RpcPacketRequest, RpcPacketResponse, GrpcStatus } from '../generated/grpc';
import { hashCode } from './hash';

export interface RPCRequestParameters {
    methodName: string;
    rpcRequest: Uint8Array;
    callId: number;
    serviceName?: string;
    channelId?: number;
}

export const createRPCRequest = (parameters: RPCRequestParameters): Uint8Array => {

    const {
        callId,
        methodName,
        rpcRequest,
        serviceName,
        channelId
    } = parameters;

    const request = RpcPacketRequest.create({
        callId,
        methodId: hashCode(methodName),
        payload: rpcRequest,
        serviceId: hashCode(serviceName),
        channelId,
    });
    return RpcPacketRequest.encode(request).finish();
}

export const parseRPCResponse = (rpcResponse: Uint8Array) => {
    const parsedResponse = RpcPacketResponse.decode(rpcResponse);
    return parsedResponse;
}
