/*
 * Copyright (c) 2018-2019 Arm Limited.
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
#include "arm_compute/graph/frontend/Stream.h"

#include "arm_compute/graph/Utils.h"
#include "arm_compute/graph/frontend/ILayer.h"

namespace arm_compute
{
namespace graph
{
namespace frontend
{



Stream::Stream(size_t id, std::string name)
    : _ctx(), _manager(), _g(id, std::move(name))
{
}

void Stream::finalize(Target target, const GraphConfig &config, std::set<int> *b, int blocking)
{
    PassManager pm = create_default_pass_manager(target, config);
    _ctx.set_config(config);
    _manager.finalize_graph(_g, _ctx, pm, target, b, blocking);
}

void Stream::measure(int n)
{
	_manager.print_times(_g, n);
}

void Stream::reset()
{
	_manager.reset(_g);
}


void Stream::run(int n)
{
    _manager.execute_graph(_g,n);
}

/*void Stream::run(double &in,double &task, double &out)
{
    _manager.execute_graph(_g,in,task,out);
}*/
void Stream::run(bool anotate, int nn)
{
	//start=std::chrono::high_resolution_clock::now();
    _manager.execute_graph(_g, anotate, nn);
    //finish=std::chrono::high_resolution_clock::now();
}



void Stream::add_layer(ILayer &layer)
{
    auto nid   = layer.create_layer(*this);
    std::cerr<<"(stream) Adding layer "<<layer.name()<<" "<<_tail_node<<"->"<<nid<<std::endl;
    _tail_node = nid;
}


const Graph &Stream::graph() const
{
    return _g;
}

Graph &Stream::graph()
{
    return _g;
}



Stream & Stream::operator<<(ILayer &layer)
{
	std::cerr<<"(stream) Layer Name:"<<layer.name()<<std::endl;
    add_layer(layer);
    return *this;
}

} // namespace frontend
} // namespace graph
} // namespace arm_compute
