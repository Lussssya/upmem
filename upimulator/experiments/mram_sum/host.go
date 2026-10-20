package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/misc"
)

type MramSum struct {
	numDpus     int
	arrayLength int64
}

func (this *MramSum) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	params := commandLineParser.DataPrepParams()

	if len(params) != 1 {
		panic(errors.New("MRAM_SUM expects one array length"))
	}
	if params[0] < 1 || int64(params[0]) > simpleSumArrayLength {
		panic(errors.New("MRAM_SUM array length must be from 1 through 256"))
	}

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.arrayLength = int64(params[0])
}

func (this *MramSum) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"array_length": simpleWord32(this.arrayLength),
	}
}

func (this *MramSum) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"output_sum": simpleWord32(this.arrayLength * (this.arrayLength + 1) / 2),
	}
}

func (this *MramSum) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleSumInput()
}

func (this *MramSum) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *MramSum) NumExecutions() int {
	return 1
}

func (this *MramSum) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}
