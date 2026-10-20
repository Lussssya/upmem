package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/misc"
)

type TaskletSum struct {
	numDpus int
}

func (this *TaskletSum) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
}

func (this *TaskletSum) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"array_length": simpleWord32(simpleSumArrayLength),
		"input_values": simpleSumInput(),
	}
}

func (this *TaskletSum) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"output_sum": simpleWord32(simpleSumExpected()),
	}
}

func (this *TaskletSum) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *TaskletSum) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *TaskletSum) NumExecutions() int {
	return 1
}

func (this *TaskletSum) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}
