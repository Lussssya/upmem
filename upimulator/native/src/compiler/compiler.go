package compiler

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"uPIMulator/src/misc"
)

type Compiler struct {
	command_line_parser *misc.CommandLineParser

	root_dirpath string
	benchmark    string

	num_dpus     int
	num_tasklets int
}

func (this *Compiler) Init(command_line_parser *misc.CommandLineParser) {
	this.command_line_parser = command_line_parser

	this.root_dirpath = command_line_parser.StringParameter("root_dirpath")
	this.benchmark = command_line_parser.StringParameter("benchmark")

	num_channels := int(command_line_parser.IntParameter("num_channels"))
	num_ranks_per_channel := int(command_line_parser.IntParameter("num_ranks_per_channel"))
	num_dpus_per_rank := int(command_line_parser.IntParameter("num_dpus_per_rank"))
	this.num_dpus = num_channels * num_ranks_per_channel * num_dpus_per_rank

	this.num_tasklets = int(command_line_parser.IntParameter("num_tasklets"))

	this.Build()
}

func (this *Compiler) Build() {
	if _, err := exec.LookPath("dpu-upmem-dpurte-clang"); err != nil {
		panic(fmt.Errorf("UPMEM compiler not found in PATH: %w", err))
	}
}

func (this *Compiler) Compile() {
	this.CompileBenchmark()
	this.CompileSdk()
}

func (this *Compiler) CompileBenchmark() {
	build_script := filepath.Join(this.root_dirpath, "benchmark", "build.py")
	command := exec.Command(
		"python3",
		build_script,
		"--benchmark",
		this.benchmark,
		"--num_dpus",
		strconv.Itoa(this.num_dpus),
		"--num_tasklets",
		strconv.Itoa(this.num_tasklets),
	)
	command.Dir = this.root_dirpath
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr

	if err := command.Run(); err != nil {
		panic(fmt.Errorf("benchmark compilation failed: %w", err))
	}
}

func (this *Compiler) CompileSdk() {
	build_script := filepath.Join(this.root_dirpath, "sdk", "build.py")
	command := exec.Command(
		"python3",
		build_script,
		"--num_tasklets",
		strconv.Itoa(this.num_tasklets),
	)
	command.Dir = this.root_dirpath
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr

	if err := command.Run(); err != nil {
		panic(fmt.Errorf("SDK compilation failed: %w", err))
	}
}
