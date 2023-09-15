import { RPCImpl } from "protobufjs";
import * as grpc from './generated/grpc';
import { createTransferService } from "./client";


describe('client', () => {
  describe('transfer service', () => {
    it('should be able to transfer a buffer correctly', async () => {

      let requestData;
      let requestMethod;

      const testImplementation: RPCImpl = (method, reqData, callback) => {
          requestData = reqData;
          requestMethod = method.name;
          const response = new grpc.Chunk({  });
          const rawResponseData = grpc.Chunk.encode(response).finish();
          callback(null, rawResponseData);
      };

      const transferService = createTransferService(testImplementation);

      const testChunk = new grpc.Chunk({
          data: Buffer.from([0x01, 0x02, 0x03]),
          offset: 0,
      });

      const result = await transferService.write(testChunk);


      console.log(result);
    });
  });
})