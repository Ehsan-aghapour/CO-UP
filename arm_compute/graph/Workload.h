/*
 * Copyright (c) 2018-2020 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef ARM_COMPUTE_GRAPH_WORKLOAD_H
#define ARM_COMPUTE_GRAPH_WORKLOAD_H

#include "arm_compute/graph/GraphContext.h"
#include "arm_compute/graph/Tensor.h"
#include "arm_compute/runtime/IFunction.h"
#include "arm_compute/runtime/IMemoryGroup.h"
#include "arm_compute/graph/TensorPipeline.h"

#include <functional>
#include <memory>
#include <vector>
//extern std::map<std::string,double> task_times;

namespace arm_compute
{
namespace graph
{
// Forward declarations
class ITensorHandle;
class INode;
class Graph;

struct ExecutionTask;

//void execute_task(ExecutionTask &task, const std::map<std::string,double>& tt=std::map<std::string, double>());
void execute_task(ExecutionTask &task);
double execute_task2(ExecutionTask &task,int nn);

/** Task executor */
class TaskExecutor final
{
private:
    /** Default constructor **/
    TaskExecutor();

public:
    /** Task executor accessor
     *
     * @return Task executor instance
     */
    static TaskExecutor &get();
    /** Function that is responsible for executing tasks */
    std::function<decltype(execute_task)> execute_function;
    std::function<decltype(execute_task2)> execute_function2;
    //std::function<void(ExecutionTask &task, const std::map<std::string,double>& tt= std::map<std::string, double>())> execute_function;
};

/** Execution task
 *
 * Contains all the information required to execute a given task
 */
struct ExecutionTask
{
    ExecutionTask(std::unique_ptr<arm_compute::IFunction> &&f, INode *n)
        : task(std::move(f)), node(n)
    {
    }
    /** Prevent instances of this class from being copied (As this class contains pointers) */
    ExecutionTask(const ExecutionTask &) = delete;
    /** Prevent instances of this class from being copied (As this class contains pointers) */
    ExecutionTask &operator=(const ExecutionTask &) = delete;
    /** Default Move Constructor. */
    ExecutionTask(ExecutionTask &&) noexcept = default;
    /** Default move assignment operator */
    ExecutionTask &operator=(ExecutionTask &&) noexcept = default;
    /** Default destructor */
    ~ExecutionTask() = default;
    // TODO (geopin01) : Support vector of functions?
    std::unique_ptr<arm_compute::IFunction> task = {}; /**< Task to execute */
    INode                                  *node = {}; /**< Node bound to this workload */

    /** Function operator */
    void operator()();
    void operator()(int nn);

    /** Prepare execution task */
    void prepare();

    double time(int n);
    void reset();

    /*DVFS */
    void set_freq(std::array<int, 3> freqs){
		LittleFreq=freqs[0];
		bigFreq=freqs[1];
		GPUFreq=freqs[2];
		return;
	}
	void apply_freq(std::string name="");
	void switch_GPIO_starting();
	void switch_GPIO_ending();
	static void init();
	static void finish();

	static int get_max_l();
	static int get_max_b();
	static int get_max_g();

	int LittleFreq=-1;
	int bigFreq=-1;
	int GPUFreq=-1;
	bool starting=false;
	//static DVFS dvfs;


    double t;
    int n=0;
    bool block=0;
    bool ending=0;
    bool governor=0;
    bool starting_gpio_switch=0;
    bool ending_gpio_switch=0;
    bool profile_layers=0;
    bool profile_transfers=0;
    char processor='B';
};

/** Execution workload */
struct ExecutionWorkload
{
    std::vector<Tensor *>      inputs  = {};          /**< Input handles */
    std::vector<Tensor *>      outputs = {};          /**< Output handles */
    std::vector<ExecutionTask> tasks   = {};          /**< Execution workload */
    Graph                     *graph   = { nullptr }; /**< Graph bound to the workload */
    GraphContext              *ctx     = { nullptr }; /**< Graph execution context */
    std::vector<TensorPipelineReceiver *>      receivers  = {};          /**< receiver handles */
    std::vector<TensorPipelineSender *>      senders = {};
};
} // namespace graph
} // namespace arm_compute
#endif /* ARM_COMPUTE_GRAPH_WORKLOAD_H */
