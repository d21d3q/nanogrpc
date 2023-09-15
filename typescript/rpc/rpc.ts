import { RpcPacketRequest, RpcPacketResponse, GrpcStatus } from '../generated/nanogrpc';
import { hashCode } from './hash';

export const createRPCRequest = (methodName: string, rpcRequest: Uint8Array, callId: number): Uint8Array => {
    const request = RpcPacketRequest.create({
        callId,
        methodId: hashCode(methodName),
        payload: rpcRequest,
    });
    return RpcPacketRequest.encodeDelimited(request).finish();
}

export const parseRPCResponse = (rpcResponse: Uint8Array) => {
    const parsedResponse = RpcPacketResponse.decodeDelimited(rpcResponse);
    return parsedResponse;
}
