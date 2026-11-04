package prim

import (
	"errors"
	"uPIMulator/src/abi/encoding"
	"uPIMulator/src/misc"
)

const scalarWorkloadCapacity = 256
const scalarWorkloadMaxUint32 int64 = 4294967295

type ScalarWorkload struct {
	numDpus    int
	memoryMode int
	expected   int64
	values     []int64
}

func (this *ScalarWorkload) Init(commandLineParser *misc.CommandLineParser) {
	numChannels := int(commandLineParser.IntParameter("num_channels"))
	numRanksPerChannel := int(commandLineParser.IntParameter("num_ranks_per_channel"))
	numDpusPerRank := int(commandLineParser.IntParameter("num_dpus_per_rank"))
	params := commandLineParser.DataPrepParams()

	if len(params) < 4 {
		panic(errors.New("SCALAR_WORKLOAD expects memory mode, expected result, count, and values"))
	}
	if params[0] != 0 && params[0] != 1 {
		panic(errors.New("SCALAR_WORKLOAD memory mode must be 0 (WRAM) or 1 (MRAM)"))
	}
	if params[1] < 0 || int64(params[1]) > scalarWorkloadMaxUint32 {
		panic(errors.New("SCALAR_WORKLOAD expected result must be uint32"))
	}

	count := params[2]
	if count < 1 || count > scalarWorkloadCapacity {
		panic(errors.New("SCALAR_WORKLOAD element count must be from 1 through 256"))
	}
	if len(params) != count+3 {
		panic(errors.New("SCALAR_WORKLOAD element count does not match the supplied values"))
	}

	this.numDpus = numChannels * numRanksPerChannel * numDpusPerRank
	this.memoryMode = params[0]
	this.expected = int64(params[1])
	this.values = make([]int64, 0, scalarWorkloadCapacity)
	for _, value := range params[3:] {
		value64 := int64(value)
		if value64 < 0 || value64 > scalarWorkloadMaxUint32 {
			panic(errors.New("SCALAR_WORKLOAD values must be uint32"))
		}
		this.values = append(this.values, value64)
	}
}

func (this *ScalarWorkload) InputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	input := map[string]*encoding.ByteStream{
		"array_length": simpleWord32(int64(len(this.values))),
	}
	if this.memoryMode == 0 {
		values := new(encoding.ByteStream)
		values.Init()
		for _, value := range this.values {
			values.Merge(simpleWord32(value))
		}
		input["input_values"] = values
	}
	return input
}

func (this *ScalarWorkload) OutputDpuHost(execution int, dpuID int) map[string]*encoding.ByteStream {
	this.validate(execution, dpuID)
	return map[string]*encoding.ByteStream{
		"output_value": simpleWord32(this.expected),
	}
}

func (this *ScalarWorkload) InputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	if this.memoryMode == 0 {
		return 0, simpleEmptyByteStream()
	}

	values := new(encoding.ByteStream)
	values.Init()
	for _, value := range this.values {
		values.Merge(simpleWord32(value))
	}
	for i := len(this.values); i < scalarWorkloadCapacity; i++ {
		values.Merge(simpleWord32(0))
	}
	return 0, values
}

func (this *ScalarWorkload) OutputDpuMramHeapPointerName(
	execution int,
	dpuID int,
) (int64, *encoding.ByteStream) {
	this.validate(execution, dpuID)
	return 0, simpleEmptyByteStream()
}

func (this *ScalarWorkload) NumExecutions() int {
	return 1
}

func (this *ScalarWorkload) validate(execution int, dpuID int) {
	if execution != 0 {
		panic(errors.New("execution must be zero"))
	}
	if dpuID < 0 || dpuID >= this.numDpus {
		panic(errors.New("DPU ID is out of range"))
	}
}
