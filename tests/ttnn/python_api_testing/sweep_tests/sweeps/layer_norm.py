# SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.

# SPDX-License-Identifier: Apache-2.0

import torch

import ttnn

from tests.ttnn.utils_for_testing import check_with_pcc
from models.utility_functions import torch_random

parameters = {
    "batch_sizes": [(1,)],
    "height": [384, 1024],
    "width": [1024, 4096],
    "use_weight_and_bias": [False, True],
    "epsilon": [1e-6, 1e-12],
    "input_dtype": [ttnn.bfloat16],
    "input_memory_config": [ttnn.DRAM_MEMORY_CONFIG],
    "output_memory_config": [ttnn.DRAM_MEMORY_CONFIG],
}


def skip(**_):
    return False


def run(
    batch_sizes,
    height,
    width,
    use_weight_and_bias,
    epsilon,
    input_dtype,
    input_memory_config,
    output_memory_config,
    *,
    device,
):
    input_shape = (*batch_sizes, height, width)

    torch_input_tensor = torch_random(input_shape, -0.1, 0.1, dtype=torch.bfloat16)
    if use_weight_and_bias:
        torch_weight = torch_random((width,), -0.1, 0.1, dtype=torch.bfloat16)
        torch_bias = torch_random((width,), -0.1, 0.1, dtype=torch.bfloat16)
    else:
        torch_weight = None
        torch_bias = None
    torch_output_tensor = torch.nn.functional.layer_norm(
        torch_input_tensor, normalized_shape=(width,), weight=torch_weight, bias=torch_bias, eps=epsilon
    )

    input_tensor = ttnn.from_torch(
        torch_input_tensor, device=device, dtype=input_dtype, memory_config=input_memory_config
    )
    if use_weight_and_bias:
        weight = ttnn.from_torch(torch_weight, device=device, dtype=input_dtype, memory_config=input_memory_config)
        bias = ttnn.from_torch(torch_bias, device=device, dtype=input_dtype, memory_config=input_memory_config)
    else:
        weight = None
        bias = None

    output_tensor = ttnn.layer_norm(
        input_tensor, weight=weight, bias=bias, epsilon=epsilon, memory_config=output_memory_config
    )
    output_tensor = ttnn.to_torch(output_tensor)

    return check_with_pcc(torch_output_tensor, output_tensor, 0.999)
