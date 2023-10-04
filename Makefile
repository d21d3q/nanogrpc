
NANO_GRPC_PROTO_DIR = $(abspath .)
NANO_GRPC_OUT_DIR = $(abspath .)


# Build rule for the protocol
protos:
	protoc --plugin=./generator/protoc-gen-grpc -I. --grpc_out=$(NANO_GRPC_OUT_DIR) ./nanogrpc.proto

all: protos