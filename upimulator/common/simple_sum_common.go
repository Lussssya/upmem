package prim

import (
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/abi/word"
)

const simpleSumArrayLength int64 = 256

func simpleSumInput() *encoding.ByteStream {
	byteStream := new(encoding.ByteStream)
	byteStream.Init()

	for value := int64(1); value <= simpleSumArrayLength; value++ {
		byteStream.Merge(simpleWord32(value))
	}

	return byteStream
}

func simpleSumExpected() int64 {
	return simpleSumArrayLength * (simpleSumArrayLength + 1) / 2
}

func simpleWord32(value int64) *encoding.ByteStream {
	valueWord := new(word.Word)
	valueWord.Init(32)
	valueWord.SetValue(value)

	return valueWord.ToByteStream()
}

func simpleEmptyByteStream() *encoding.ByteStream {
	byteStream := new(encoding.ByteStream)
	byteStream.Init()
	return byteStream
}
