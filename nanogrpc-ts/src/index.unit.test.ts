// import { RPCImpl } from "protobufjs";
// import * as grpc from './generated/nanogrpc';
// import { createRPCImplementation } from ".";


// describe('client', () => {
//   describe('transfer service', () => {
//     it('should be able to transfer a buffer correctly', async () => {

//       let reqPayload;
//       let reqMethod;

//       const testAdapter = {
//         write: async (data: any) => {

//           const request = grpc.RpcPacketRequest.decode(data);

//           reqMethod = request.methodId;
//           reqPayload = request.payload;

//           const response = new grpc.RpcPacketResponse({
//             type: grpc.PacketType.RESPONSE,
//             callId: request.callId,
//           });

//           return grpc.RpcPacketResponse.encode(response).finish();
//         }
//       }

//       const testImplementation = createRPCImplementation(testAdapter);

      

//     });
//   });
// })