//
// Copyright (c) 2017 The Khronos Group Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "../../test_common/harness/errorHelpers.h"
#include "../../test_common/harness/kernelHelpers.h"
#include "../../test_common/harness/typeWrappers.h"
#include "../../test_common/harness/mt19937.h"

extern int		test_multiple_contexts_same_device(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);
extern int		test_two_contexts_same_device(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);
extern int		test_three_contexts_same_device(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);
extern int		test_four_contexts_same_device(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);

extern int		test_two_devices(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);
extern int		test_max_devices(cl_device_id deviceID, cl_context context, cl_command_queue queue, int num_elements);

extern int		test_hundred_queues(cl_device_id device, cl_context context, cl_command_queue queue, int num_elements);


