

export class RPCError extends Error {
  rpcErrorCode: number;

  constructor(rpcErrorCode: number, message?: string) {
    super(message);
    this.rpcErrorCode = rpcErrorCode;
  }
}