/*
 * Copyright (c) 2018 Arm Limited.
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
#include "arm_compute/graph/backends/NPU/NPUSubTensorHandle.h"

namespace arm_compute
{
namespace graph
{
namespace backends
{
NPUSubTensorHandle::NPUSubTensorHandle(ITensorHandle *parent_handle, const TensorShape &shape, const Coordinates &coords, bool extend_parent)
    : _sub_tensor(), _parent_handle(nullptr)
{
    ARM_COMPUTE_ERROR_ON(!parent_handle);
    _sub_tensor    = arm_compute::SubTensor(&parent_handle->tensor(), shape, coords, extend_parent);
    _parent_handle = parent_handle;
}

void NPUSubTensorHandle::allocate()
{
    // noop
}

void NPUSubTensorHandle::free()
{
    // noop
}

void NPUSubTensorHandle::manage(IMemoryGroup *mg)
{
    ARM_COMPUTE_UNUSED(mg);
    // noop
}

void NPUSubTensorHandle::map(bool blocking)
{
    ARM_COMPUTE_UNUSED(blocking);
}

void NPUSubTensorHandle::unmap()
{
    // noop
}

void NPUSubTensorHandle::release_if_unused()
{
    // noop
}

const arm_compute::ITensor &NPUSubTensorHandle::tensor() const
{
    return _sub_tensor;
}

arm_compute::ITensor &NPUSubTensorHandle::tensor()
{
    return _sub_tensor;
}

ITensorHandle *NPUSubTensorHandle::parent_handle()
{
    ARM_COMPUTE_ERROR_ON(_parent_handle == nullptr);
    return _parent_handle->parent_handle();
}

bool NPUSubTensorHandle::is_subtensor() const
{
    return true;
}

Target NPUSubTensorHandle::target() const
{
    return Target::NEON;
}
} // namespace backends
} // namespace graph
} // namespace arm_compute
