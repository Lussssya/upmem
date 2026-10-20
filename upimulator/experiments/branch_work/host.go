package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/misc"
)

type BranchWork struct {
	numDpus  int
	selector int64
	work     int64
}

func (this *BranchWork) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	params := commandLineParser.DataPrepParams()

	if len(params) != 1 {
		panic(errors.New("BRANCH_WORK expects one branch selector"))
	}

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.selector = int64(params[0])
	if this.selector == 0 {
		this.work = 10
	} else {
		this.work = 1000
	}
}

func (this *BranchWork) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"branch_selector": simpleWord32(this.selector),
	}
}

func (this *BranchWork) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"output_value": simpleWord32(this.work),
	}
}

func (this *BranchWork) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *BranchWork) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *BranchWork) NumExecutions() int {
	return 1
}

func (this *BranchWork) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}
