# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
# pylint: disable=invalid-name,unused-variable,unused-argument,no-member
"""Conv2D alter op and legalize functions for x86"""

import logging

import re
import tvm
from tvm import te
from tvm import relay
from tvm import autotvm
from .conv2d_nchw_winograd import _infer_tile_size
from ..utils import get_const_tuple
from ..nn import conv2d_alter_layout

logger = logging.getLogger("topi")

_NCHWc_matcher = re.compile("^NCHW[0-9]+c$")
_OIHWo_matcher = re.compile("^OIHW[0-9]+o$")
_NHWCc_matcher = re.compile("^NHWC[0-9]+c$")
_HWIOo_matcher = re.compile("^HWIO[0-9]+o$")
_HWOIo_matcher = re.compile("^HWOI[0-9]+o$")


@conv2d_alter_layout.register("adreno")
def _alter_conv2d_layout(attrs, inputs, tinfos, out_type):
    target = tvm.target.Target.current(allow_none=False)
    dispatch_ctx = autotvm.task.DispatchContext.current
    new_attrs = {k: attrs[k] for k in attrs.keys()}

    # Parse the attributes.
    padding = attrs.get_int_tuple("padding")
    strides = attrs.get_int_tuple("strides")
    dilation = attrs.get_int_tuple("dilation")
    data_layout = attrs["data_layout"]
    kernel_layout = attrs["kernel_layout"]
    data_tensor, kernel_tensor = tinfos
    data_dtype = data_tensor.dtype
    out_dtype = out_type.dtype

    if isinstance(dispatch_ctx, autotvm.task.ApplyGraphBest):
        cfg = dispatch_ctx.query(target, None)
        workload = cfg.workload
    else:
        impl, outs = relay.backend.compile_engine.select_implementation(
            relay.op.get("nn.conv2d"), attrs, tinfos, out_type, target
        )
        workload = autotvm.task.get_workload(outs)
        if workload is None:
            if impl.name.find("winograd") != -1:
                if dilation != (1, 1):
                    logger.warning("Does not support weight pre-transform for dilated convolution.")
                    return None

                assert data_layout == "NCHW" and kernel_layout == "OIHW"
                N, CI, H, W = get_const_tuple(data_tensor.shape)
                CO, _, KH, KW = get_const_tuple(kernel_tensor.shape)

                # Pre-compute weight transformation in winograd
                tile_size = _infer_tile_size(data_tensor)

                # alpha, alpha, CO, CI
                weight = relay.nn.contrib_conv2d_winograd_weight_transform(
                    inputs[1], tile_size=tile_size
                )
                new_attrs["tile_size"] = tile_size
                new_attrs["channels"] = CO
                return relay.nn.contrib_conv2d_winograd_without_weight_transform(
                    inputs[0], weight, **new_attrs
                )
            return None

        cfg = dispatch_ctx.query(target, workload)

    topi_tmpl = workload[0]

    if "conv2d_nchw_winograd" in topi_tmpl:
        suffix = "_acc_32" if "acc_32" in topi_tmpl else ""
        wkl_name = "conv2d_nchw_winograd_without_weight_transform" + suffix + ".image2d"
        if dilation != (1, 1):
            logger.warning("Does not support weight pre-transform for dilated convolution.")
            return None

        tile_size = _infer_tile_size(data_tensor)
        if len(data_tensor.shape) == 5:
            assert data_layout == "NCHW4c" and kernel_layout == "OIHW4o"
            N, CI, H, W, CB = get_const_tuple(data_tensor.shape)
            CO, _, KH, KW, COB = get_const_tuple(kernel_tensor.shape)
            weight = relay.layout_transform(inputs[1], "OIHW4o", "OIHW")
            weight = relay.nn.contrib_conv2d_winograd_weight_transform(weight, tile_size=tile_size)
            weight = relay.layout_transform(weight, "HWOI", "HWIO4o")

            new_attrs["tile_size"] = tile_size
            new_attrs["channels"] = CO * COB

            new_data = data_tensor
            new_weight = te.placeholder(
                (KH + tile_size - 1, KW + tile_size - 1, CI * CB, CO, COB),
                dtype=kernel_tensor.dtype,
            )
            new_workload = autotvm.task.args_to_workload(
                [new_data, new_weight, strides, padding, dilation, out_dtype],
                wkl_name,
            )
            dispatch_ctx.update(target, new_workload, cfg)
            return relay.nn.contrib_conv2d_winograd_without_weight_transform(
                inputs[0], weight, **new_attrs
            )

        assert data_layout == "NCHW" and kernel_layout == "OIHW"
        N, CI, H, W = get_const_tuple(data_tensor.shape)
        CO, _, KH, KW = get_const_tuple(kernel_tensor.shape)

        # pre-compute weight transformation in winograd
        #weight = relay.nn.contrib_conv2d_winograd_weight_transform(inputs[1], tile_size=tile_size)
        #weight = relay.transpose(weight, axes=[0, 1, 3, 2]) # HWOI -> HWIO
        new_attrs["tile_size"] = tile_size
        new_attrs["channels"] = CO

        # Store the same config for the altered operator (workload)
        new_data = data_tensor
        new_weight = te.placeholder(
            (KH + tile_size - 1, KW + tile_size - 1, CI, CO), dtype=kernel_tensor.dtype
        )
        in_channel_block = CI % 4
        if in_channel_block == 0:
            in_channel_block = 4
        num_filter_block = CO % 4
        if num_filter_block == 0:
            num_filter_block = 4

        if in_channel_block != 4 or num_filter_block != 4:
            new_workload = autotvm.task.args_to_workload(
                [new_data, new_weight, strides, padding, dilation, out_dtype],
                wkl_name,
            )
            dispatch_ctx.update(target, new_workload, cfg)
            return relay.nn.contrib_conv2d_winograd_without_weight_transform(
                inputs[0], weight, **new_attrs
            )

        print("Kernel layout: ", kernel_layout)
        new_attrs["data_layout"] = "NCHW%dc" % in_channel_block
        # (oc, ic, h, w) -> (OC, IC, h, w, ic, oc)
        # TODO: @echuraev: It should be HWIO4o instead of OIHW4w
        #new_attrs["kernel_layout"] = "OIHW%dw" % num_filter_block
        new_attrs["kernel_layout"] = "HWIO%do" % num_filter_block
        #new_attrs["kernel_layout"] = "HWIO%do" % num_filter_block
        new_attrs["out_layout"] = "NCHW%dc" % num_filter_block
        # Store altered operator's config
        new_data = te.placeholder(
            (N, CI // in_channel_block, H, W, in_channel_block), dtype=data_dtype
        )
        new_weight = te.placeholder(
            (KH + tile_size - 1, KW + tile_size - 1, CI, CO // num_filter_block, num_filter_block),
            dtype=kernel_tensor.dtype,
        )
        new_workload = autotvm.task.args_to_workload(
            [
                new_data,
                new_weight,
                strides,
                padding,
                dilation,
                out_dtype,
            ],
            wkl_name,
        )
        dispatch_ctx.update(target, new_workload, cfg)
        return relay.nn.contrib_conv2d_winograd_without_weight_transform(
            inputs[0], weight, **new_attrs
        )

    if "conv2d_nchwc" in topi_tmpl: # covers both conv2d_nchwc and depthwise_conv2d_nchwc
        # we only convert conv2d_NCHW to conv2d_NCHWc for x86
        if data_layout == "NCHW" and kernel_layout == "OIHW":
            batch, in_channels, in_height, in_width = data_tensor.shape
            out_channles, _, kernel_h, kernel_w = kernel_tensor.shape
            in_channel_block = in_channels % 4
            if in_channel_block == 0:
                in_channel_block = 4
            num_filter_block = out_channles % 4
            if num_filter_block == 0:
                num_filter_block = 4

            if in_channel_block != 4 or num_filter_block != 4:
              return None

            batch_size, in_channel, height, width = get_const_tuple(data_tensor.shape)
            out_channel, in_filter_channel, kh, kw = get_const_tuple(kernel_tensor.shape)

            # update new attrs
            new_attrs["channels"] = out_channel
            new_attrs["data_layout"] = "NCHW%dc" % in_channel_block
            # (oc, ic, h, w) -> (OC, IC, h, w, ic, oc)
            new_attrs["kernel_layout"] = "OIHW%do" % num_filter_block
            new_attrs["out_layout"] = "NCHW%dc" % num_filter_block

            # Store altered operator's config
            new_data = te.placeholder(
                (batch_size, in_channel // in_channel_block, height, width, in_channel_block), dtype=data_dtype
            )
            new_kernel = te.placeholder(
                (out_channel // num_filter_block, in_filter_channel, kh, kw, num_filter_block),
                dtype=kernel_tensor.dtype,
            )
            new_workload = autotvm.task.args_to_workload(
                [
                    new_data,
                    new_kernel,
                    strides,
                    padding,
                    dilation,
                    out_dtype,
                ],
                topi_tmpl #"conv2d_nchwc.image2d",
            )
            dispatch_ctx.update(target, new_workload, cfg)
        else:
            assert _NCHWc_matcher.match(data_layout)
            assert _OIHWo_matcher.match(kernel_layout)
        return relay.nn.conv2d(*inputs, **new_attrs)


    if "conv2d_nhwc" in topi_tmpl: # covers both conv2d_nchwc and depthwise_conv2d_nchwc
        # we only convert conv2d_NCHW to conv2d_NCHWc for x86
        if ((data_layout == "NHWC" and kernel_layout == "HWIO") or
           (data_layout == "NHWC" and kernel_layout == "HWOI")):
            if kernel_layout == "HWIO":
                batch_size, in_height, in_width, in_channels = data_tensor.shape
                kernel_h, kernel_w, in_filter_channel, out_channles = kernel_tensor.shape
            else:
                batch_size, in_height, in_width, in_channels = data_tensor.shape
                kernel_h, kernel_w, out_channles, in_filter_channel = kernel_tensor.shape
            in_channel_block = in_channels % 4
            if in_channel_block == 0:
                in_channel_block = 4
            num_filter_block = out_channles % 4
            if num_filter_block == 0:
                num_filter_block = 4
            if in_channel_block != 4 or num_filter_block != 4:
              return None

            # update new attrs
            new_attrs["channels"] = out_channles
            new_attrs["data_layout"] = "NHWC%dc" % in_channel_block
            # (oc, ic, h, w) -> (OC, IC, h, w, ic, oc)
            if kernel_layout == "HWIO":
                new_attrs["kernel_layout"] = "HWIO%do" % num_filter_block
            else:
                new_attrs["kernel_layout"] = "HWOI%do" % num_filter_block
            new_attrs["out_layout"] = "NHWC%dc" % num_filter_block

            # Store altered operator's config
            new_data = te.placeholder(
                (batch_size, in_height, in_width, in_channels // in_channel_block, in_channel_block), dtype=data_dtype
            )
            if kernel_layout == "HWIO":
                new_kernel = te.placeholder(
                    (kernel_h, kernel_w, in_filter_channel, out_channles // num_filter_block, num_filter_block),
                    dtype=kernel_tensor.dtype,
                )
            else:
                new_kernel = te.placeholder(
                    (kernel_h, kernel_w, out_channles // num_filter_block, in_filter_channel, num_filter_block),
                    dtype=kernel_tensor.dtype,
                )
            new_workload = autotvm.task.args_to_workload(
                [
                    new_data,
                    new_kernel,
                    strides,
                    padding,
                    dilation,
                    out_dtype,
                ],
                topi_tmpl,
            )
            dispatch_ctx.update(target, new_workload, cfg)
        else:
            assert _NHWCc_matcher.match(data_layout)
            assert (_HWIOo_matcher.match(kernel_layout) or _HWOIo_matcher.match(kernel_layout))
        return relay.nn.conv2d(*inputs, **new_attrs)

    return None


