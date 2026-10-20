package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/abi/word"
	"uPIMulator/src/misc"
)

type LoopAdd struct {
	numDpus      int
	initialValue int64
	iterations   int64
}

func (this *LoopAdd) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	params := commandLineParser.DataPrepParams()

	if len(params) != 2 {
		panic(errors.New("LOOP_ADD expects initial value and iteration count"))
	}
	if params[1] < 0 {
		panic(errors.New("LOOP_ADD iteration count must be non-negative"))
	}

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.initialValue = int64(params[0])
	this.iterations = int64(params[1])
}

func (this *LoopAdd) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)

	return map[string]*encoding.ByteStream{
		"input_value": this.word32(this.initialValue),
		"iterations":  this.word32(this.iterations),
	}
}

func (this *LoopAdd) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)

	return map[string]*encoding.ByteStream{
		"output_value": this.word32(this.initialValue + this.iterations),
	}
}

func (this *LoopAdd) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)

	return 0, this.emptyByteStream()
}

func (this *LoopAdd) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)

	return 0, this.emptyByteStream()
}

func (this *LoopAdd) NumExecutions() int {
	return 1
}

func (this *LoopAdd) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}

func (this *LoopAdd) word32(value int64) *encoding.ByteStream {
	valueWord := new(word.Word)
	valueWord.Init(32)
	valueWord.SetValue(value)

	return valueWord.ToByteStream()
}

func (this *LoopAdd) emptyByteStream() *encoding.ByteStream {
	byteStream := new(encoding.ByteStream)
	byteStream.Init()

	return byteStream
}
