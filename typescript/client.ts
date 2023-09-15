import { RPCImpl } from 'protobufjs';
import * as grpc from './generated/grpc';




export const createTransferService = (rpcImpl: RPCImpl) => {
    const service = grpc.Transfer.create(rpcImpl);
    return service;
}