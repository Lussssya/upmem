package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/abi/word"
	"uPIMulator/src/misc"
)

type Increment struct {
	numDpus int
	input   int64
}

func (this *Increment) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.input = int64(commandLineParser.DataPrepParams()[0])
}

func (this *Increment) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)

	return map[string]*encoding.ByteStream{"input_value": this.word32(this.input)}
}

func (this *Increment) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)

	return map[string]*encoding.ByteStream{"output_value": this.word32(this.input + 1)}
}

func (this *Increment) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)

	return 0, this.emptyByteStream()
}

func (this *Increment) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)

	return 0, this.emptyByteStream()
}

func (this *Increment) NumExecutions() int {
	return 1
}

func (this *Increment) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}

func (this *Increment) word32(value int64) *encoding.ByteStream {
	valueWord := new(word.Word)
	valueWord.Init(32)
	valueWord.SetValue(value)

	return valueWord.ToByteStream()
}

func (this *Increment) emptyByteStream() *encoding.ByteStream {
	byteStream := new(encoding.ByteStream)
	byteStream.Init()

	return byteStream
}
