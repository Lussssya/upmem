package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/misc"
)

const distributedSumMramCapacity = 256
const distributedSumMramMaxUint32 int64 = 4294967295

type DistributedSumMram struct {
	numDpus int
	values  []int64
	sum     int64
}

func (this *DistributedSumMram) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	params := commandLineParser.DataPrepParams()

	if len(params) < 2 {
		panic(errors.New("DISTRIBUTED_SUM_MRAM expects a count followed by input values"))
	}

	count := params[0]
	if count < 1 || count > distributedSumMramCapacity {
		panic(errors.New("DISTRIBUTED_SUM_MRAM element count must be from 1 through 256"))
	}
	if len(params) != count+1 {
		panic(errors.New("DISTRIBUTED_SUM_MRAM element count does not match the supplied values"))
	}

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.values = make([]int64, 0, distributedSumMramCapacity)
	this.sum = 0

	for _, value := range params[1:] {
		value64 := int64(value)
		if value64 < 0 || value64 > distributedSumMramMaxUint32 {
			panic(errors.New("DISTRIBUTED_SUM_MRAM values must be uint32"))
		}

		this.sum += value64
		if this.sum > distributedSumMramMaxUint32 {
			panic(errors.New("DISTRIBUTED_SUM_MRAM result must fit in uint32"))
		}

		this.values = append(this.values, value64)
	}
}

func (this *DistributedSumMram) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"array_length": simpleWord32(int64(len(this.values))),
	}
}

func (this *DistributedSumMram) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"output_sum": simpleWord32(this.sum),
	}
}

func (this *DistributedSumMram) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)

	values := new(encoding.ByteStream)
	values.Init()
	for _, value := range this.values {
		values.Merge(simpleWord32(value))
	}
	for i := len(this.values); i < distributedSumMramCapacity; i++ {
		values.Merge(simpleWord32(0))
	}

	return 0, values
}

func (this *DistributedSumMram) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *DistributedSumMram) NumExecutions() int {
	return 1
}

func (this *DistributedSumMram) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}
