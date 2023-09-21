import { RPCImpl } from 'protobufjs';
import { RpcPacketRequest, RpcPacketResponse, GrpcStatus } from '../generated/grpc';

import { createRPCRequest, parseRPCResponse } from './rpc';

describe('rpc', () => {
    describe('grpc implementation', () => {
        describe('request', () => {
            it('can create a correct RPC request', () => {
                const testData = Buffer.from([0x01, 0x02, 0x03]);

                const result = createRPCRequest({ methodName: 'Something',rpcRequest: testData, callId: 887 });
    
                const parsedResult = RpcPacketRequest.decode(result);
                
                expect(parsedResult.callId).toEqual(887);
                expect(parsedResult.methodId).toEqual(1439082586);
                expect(parsedResult.payload).toEqual(testData);
            })
        });
        describe('response', () => {
            it('can parse a correct RPC response', () => {

                const testResponse = RpcPacketResponse.encode(RpcPacketResponse.create({
                    callId: 998,
                    grpcStatus: GrpcStatus.OK,
                    data: Buffer.from([0x01, 0x02, 0x03])
                })).finish();

                const result = parseRPCResponse(testResponse);

                expect(result.callId).toEqual(998);
                expect(result.status).toEqual(GrpcStatus.OK);
            });
        });
    })
});