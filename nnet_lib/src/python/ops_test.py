#!/usr/bin/env python

"""Tests for python/ops.py."""

import unittest
import smaug_test
from graph import *
from tensor import *
from ops import *
from types_pb2 import *

test_backend_dtypes = {"Reference": np.float32, "SMV": np.float16}

class OperatorTest:
  def assertEqualDims(self, dims, nchw_dims, layout):
    """Test equality between two dims.

    Test if the two dims share the same dimensions or are merely one single
    shape in different layouts.

    Args:
      dims: A list of dimensions.
      nchw_dims: A list of dimensions in NCHW layout.
      layout: The layout of dims.
    """
    if layout == NCHW:
      self.assertEqual(dims, nchw_dims)
    elif layout == NHWC:
      self.assertEqual([dims[0], dims[3], dims[1], dims[2]], nchw_dims)
    else:
      assert False, "Other layouts not expected here!"

  def build_test_sequential_graph(self, backend):
    """Create a sequential model."""
    np_dtype = test_backend_dtypes[backend]
    self.expected_dtype = np_to_smaug_type[np_dtype]
    with Graph(name="test_sequential_graph", backend=backend) as graph:
      input_tensor = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(1, 3, 28, 28).astype(np_dtype))
      filter_tensor0 = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(64, 3, 3, 3).astype(np_dtype))
      filter_tensor1 = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(64, 64, 3, 3).astype(np_dtype))
      weight_tensor0 = Tensor(
          data_layout=NC,
          tensor_data=np.random.rand(12544, 254).astype(np_dtype))
      weight_tensor1 = Tensor(
          data_layout=NC, tensor_data=np.random.rand(254, 10).astype(np_dtype))
      bn_mean_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_var_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_gamma_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_beta_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))

      out = input_data("input", input_tensor)
      out = convolution(
          "conv0", out, filter_tensor0, stride=[1, 1], padding="same")
      out = relu("conv0_relu", out)
      out = batch_norm("bn", out, bn_mean_tensor, bn_var_tensor,
                       bn_gamma_tensor, bn_beta_tensor)
      out = convolution(
          "conv1", out, filter_tensor1, stride=[1, 1], padding="same")
      out = relu("conv1_relu", out)
      out = max_pool("pool", out, pool_size=[2, 2], stride=[2, 2])
      out = flatten("flatten", out)
      out = mat_mul("fc0", out, weight_tensor0)
      out = relu("fc0_relu", out)
      out = mat_mul("fc1", out, weight_tensor1)

    self.test_graph = graph
    self.backend = backend
    self.alignment = backend_alignment[self.test_graph.graph.backend]

  def build_test_residual_graph(self, backend):
    """Create a residual model.

    The graph contains a residual connection, where the output of conv0 and
    conv2 is added at the end."""

    np_dtype = test_backend_dtypes[backend]
    self.expected_dtype = np_to_smaug_type[np_dtype]
    with Graph(name="test_residual_graph", backend=backend) as graph:
      input_tensor = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(1, 1, 28, 28).astype(np_dtype))
      filter_tensor0 = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(64, 1, 3, 3).astype(np_dtype))
      filter_tensor1 = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(64, 1, 3, 3).astype(np_dtype))
      filter_tensor2 = Tensor(
          data_layout=NCHW,
          tensor_data=np.random.rand(64, 64, 3, 3).astype(np_dtype))
      bn_mean_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_var_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_gamma_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))
      bn_beta_tensor = Tensor(
          data_layout=X, tensor_data=np.random.rand(64).astype(np_dtype))

      act = input_data("input", input_tensor)
      x = convolution(
          "conv0", act, filter_tensor0, stride=[1, 1], padding="same")
      out = convolution(
          "conv1", act, filter_tensor1, stride=[1, 1], padding="same")
      out = batch_norm("bn", out, bn_mean_tensor, bn_var_tensor,
                       bn_gamma_tensor, bn_beta_tensor)
      out = relu("relu", out)
      out = convolution(
          "conv2", out, filter_tensor2, stride=[1, 1], padding="same")
      out = add("add", x, out)

    self.test_graph = graph
    self.backend = backend
    self.alignment = backend_alignment[self.test_graph.graph.backend]

class SequentialGraphTest(OperatorTest):
  """Common tests for the sequential graph."""

  def test_input_op(self):
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(node.op, Data)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "input")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 3, 28, 28])
    self.assertEqual(node.output_tensors[0].shape.layout, NCHW)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_convolution_op(self):
    expected_input_layout = backend_layouts[
        self.backend][Convolution3d].input_layoutset.layouts
    expected_output_layout = backend_layouts[
        self.backend][Convolution3d].output_layoutset.layouts
    # The first convolution operator "conv0"
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.op, Convolution3d)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.conv_params.padding, SamePadding)
    self.assertEqual(node.params.conv_params.stride, [1, 1])
    # Filter tensor
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqualDims(node.input_tensors[1].shape.dims, [64, 3, 3, 3],
                         expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.layout, expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv0")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The second convolution operator "conv1"
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.op, Convolution3d)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.conv_params.padding, SamePadding)
    self.assertEqual(node.params.conv_params.stride, [1, 1])
    # Filter tensor
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqualDims(node.input_tensors[1].shape.dims, [64, 64, 3, 3],
                         expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.layout, expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv1")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_relu_op(self):
    # The first relu operator "conv0_relu"
    node = self.get_node(self.test_graph.graph, "conv0_relu")
    self.assertEqual(node.op, ReLU)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv0_relu")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The second relu operator "conv1_relu"
    node = self.get_node(self.test_graph.graph, "conv1_relu")
    self.assertEqual(node.op, ReLU)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv1_relu")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The third relu operator "fc0_relu"
    node = self.get_node(self.test_graph.graph, "fc0_relu")
    self.assertEqual(node.op, ReLU)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "fc0_relu")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 254])
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_batch_norm_op(self):
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.op, BatchNorm)
    self.assertEqual(len(node.input_tensors), 5)
    self.assertEqual(len(node.output_tensors), 1)
    # Weight tensors
    self.assertEqual(node.input_tensors[1].name, "bn/mean")
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[1].shape.dims, [64])
    self.assertEqual(node.input_tensors[1].shape.layout, X)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[2].name, "bn/var")
    self.assertEqual(node.input_tensors[2].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[2].shape.dims, [64])
    self.assertEqual(node.input_tensors[2].shape.layout, X)
    self.assertEqual(node.input_tensors[2].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[3].name, "bn/gamma")
    self.assertEqual(node.input_tensors[3].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[3].shape.dims, [64])
    self.assertEqual(node.input_tensors[3].shape.layout, X)
    self.assertEqual(node.input_tensors[3].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[4].name, "bn/beta")
    self.assertEqual(node.input_tensors[4].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[4].shape.dims, [64])
    self.assertEqual(node.input_tensors[4].shape.layout, X)
    self.assertEqual(node.input_tensors[4].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "bn")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 64, 28, 28])
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_max_pool_op(self):
    expected_output_layout = backend_layouts[
        self.backend][MaxPooling].output_layoutset.layouts
    node = self.get_node(self.test_graph.graph, "pool")
    self.assertEqual(node.op, MaxPooling)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.pool_params.stride, [2, 2])
    self.assertEqual(node.params.pool_params.pool_size, [2, 2])
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "pool")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 14, 14],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_flatten_op(self):
    node = self.get_node(self.test_graph.graph, "flatten")
    self.assertEqual(node.op, Reorder)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "flatten")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 12544])
    self.assertEqual(node.output_tensors[0].shape.layout, NC)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_mat_mul_op(self):
    # The first mat_mul operator "fc0"
    node = self.get_node(self.test_graph.graph, "fc0")
    self.assertEqual(node.op, InnerProduct)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Weight tensor
    self.assertEqual(node.input_tensors[1].name, "fc0/weights")
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[1].shape.dims, [12544, 254])
    self.assertEqual(node.input_tensors[1].shape.layout, NC)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "fc0")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 254])
    self.assertEqual(node.output_tensors[0].shape.layout, NC)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The second mat_mul operator "fc1"
    node = self.get_node(self.test_graph.graph, "fc1")
    self.assertEqual(node.op, InnerProduct)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Weight tensor
    self.assertEqual(node.input_tensors[1].name, "fc1/weights")
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[1].shape.dims, [254, 10])
    self.assertEqual(node.input_tensors[1].shape.layout, NC)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "fc1")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 10])
    self.assertEqual(node.output_tensors[0].shape.layout, NC)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

class ResidualGraphTest(OperatorTest):
  """Common tests for the residual graph."""

  def test_input_op(self):
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(node.op, Data)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "input")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 1, 28, 28])
    self.assertEqual(node.output_tensors[0].shape.layout, NCHW)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_convolution_op(self):
    expected_input_layout = backend_layouts[
        self.backend][Convolution3d].input_layoutset.layouts
    expected_output_layout = backend_layouts[
        self.backend][Convolution3d].output_layoutset.layouts
    # The first convolution operator "conv0"
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.op, Convolution3d)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.conv_params.padding, SamePadding)
    self.assertEqual(node.params.conv_params.stride, [1, 1])
    # Filter tensor
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqualDims(node.input_tensors[1].shape.dims, [64, 1, 3, 3],
                         expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.layout, expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv0")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The second convolution operator "conv1"
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.op, Convolution3d)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.conv_params.padding, SamePadding)
    self.assertEqual(node.params.conv_params.stride, [1, 1])
    # Filter tensor
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqualDims(node.input_tensors[1].shape.dims, [64, 1, 3, 3],
                         expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.layout, expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv1")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

    # The third convolution operator "conv2"
    node = self.get_node(self.test_graph.graph, "conv2")
    self.assertEqual(node.op, Convolution3d)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Parameters
    self.assertEqual(node.params.conv_params.padding, SamePadding)
    self.assertEqual(node.params.conv_params.stride, [1, 1])
    # Filter tensor
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqualDims(node.input_tensors[1].shape.dims, [64, 64, 3, 3],
                         expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.layout, expected_input_layout)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "conv2")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     expected_output_layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_relu_op(self):
    node = self.get_node(self.test_graph.graph, "relu")
    self.assertEqual(node.op, ReLU)
    self.assertEqual(len(node.input_tensors), 1)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "relu")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_batch_norm_op(self):
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.op, BatchNorm)
    self.assertEqual(len(node.input_tensors), 5)
    self.assertEqual(len(node.output_tensors), 1)
    # Weight tensors
    self.assertEqual(node.input_tensors[1].name, "bn/mean")
    self.assertEqual(node.input_tensors[1].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[1].shape.dims, [64])
    self.assertEqual(node.input_tensors[1].shape.layout, X)
    self.assertEqual(node.input_tensors[1].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[2].name, "bn/var")
    self.assertEqual(node.input_tensors[2].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[2].shape.dims, [64])
    self.assertEqual(node.input_tensors[2].shape.layout, X)
    self.assertEqual(node.input_tensors[2].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[3].name, "bn/gamma")
    self.assertEqual(node.input_tensors[3].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[3].shape.dims, [64])
    self.assertEqual(node.input_tensors[3].shape.layout, X)
    self.assertEqual(node.input_tensors[3].shape.alignment, self.alignment)
    self.assertEqual(node.input_tensors[4].name, "bn/beta")
    self.assertEqual(node.input_tensors[4].data_type, self.expected_dtype)
    self.assertEqual(node.input_tensors[4].shape.dims, [64])
    self.assertEqual(node.input_tensors[4].shape.layout, X)
    self.assertEqual(node.input_tensors[4].shape.alignment, self.alignment)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "bn")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqual(node.output_tensors[0].shape.dims, [1, 64, 28, 28])
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

  def test_add_op(self):
    node = self.get_node(self.test_graph.graph, "add")
    self.assertEqual(node.op, EltwiseAdd)
    self.assertEqual(len(node.input_tensors), 2)
    self.assertEqual(len(node.output_tensors), 1)
    # Output tensor
    self.assertEqual(node.output_tensors[0].name, "add")
    self.assertEqual(node.output_tensors[0].data_type, self.expected_dtype)
    self.assertEqualDims(node.output_tensors[0].shape.dims, [1, 64, 28, 28],
                         node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.layout,
                     node.input_tensors[0].shape.layout)
    self.assertEqual(node.output_tensors[0].shape.alignment, self.alignment)

class SMVSequentialGraphTest(smaug_test.SmaugTest, SequentialGraphTest):
  """Test the sequential graph on the SMV backend."""

  def __init__(self, *args, **kwargs):
    super(SMVSequentialGraphTest, self).__init__(*args, **kwargs)
    self.build_test_sequential_graph("SMV")

  def test_parent_children(self):
    """Test the parent/child relationship in the graph.

    Because different backends may require different layout transformations
    between layers, so we delete this test from the above common tests.
    """
    # input (Data).
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(len(node.parents), 0)
    self.assertEqual(node.children[0], "input->conv0")
    # Reorder input from NCHW to NHWC.
    node = self.get_node(self.test_graph.graph, "input->conv0")
    self.assertEqual(node.parents[0], "input")
    self.assertEqual(node.children[0], "conv0")
    # conv0 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.parents[0], "input->conv0")
    self.assertEqual(node.children[0], "conv0_relu")
    # conv0_relu (ReLU).
    node = self.get_node(self.test_graph.graph, "conv0_relu")
    self.assertEqual(node.parents[0], "conv0")
    self.assertEqual(node.children[0], "conv0_relu->bn")
    # Reorder conv0_relu from NHWC to HCHW.
    node = self.get_node(self.test_graph.graph, "conv0_relu->bn")
    self.assertEqual(node.parents[0], "conv0_relu")
    self.assertEqual(node.children[0], "bn")
    # bn (BN).
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.parents[0], "conv0_relu->bn")
    self.assertEqual(node.children[0], "bn->conv1")
    # Reorder bn from NCHW to NHWC.
    node = self.get_node(self.test_graph.graph, "bn->conv1")
    self.assertEqual(node.parents[0], "bn")
    self.assertEqual(node.children[0], "conv1")
    # conv1 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.parents[0], "bn->conv1")
    self.assertEqual(node.children[0], "conv1_relu")
    # conv1_relu (ReLU).
    node = self.get_node(self.test_graph.graph, "conv1_relu")
    self.assertEqual(node.parents[0], "conv1")
    self.assertEqual(node.children[0], "pool")
    # pool (MaxPooling).
    node = self.get_node(self.test_graph.graph, "pool")
    self.assertEqual(node.parents[0], "conv1_relu")
    self.assertEqual(node.children[0], "flatten")
    # flatten (Flatten).
    node = self.get_node(self.test_graph.graph, "flatten")
    self.assertEqual(node.parents[0], "pool")
    self.assertEqual(node.children[0], "fc0")
    # fc0 (FC).
    node = self.get_node(self.test_graph.graph, "fc0")
    self.assertEqual(node.parents[0], "flatten")
    self.assertEqual(node.children[0], "fc0_relu")
    # fc0_relu (ReLU)
    node = self.get_node(self.test_graph.graph, "fc0_relu")
    self.assertEqual(node.parents[0], "fc0")
    self.assertEqual(node.children[0], "fc1")
    # fc1 (FC).
    node = self.get_node(self.test_graph.graph, "fc1")
    self.assertEqual(node.parents[0], "fc0_relu")
    self.assertEqual(len(node.children), 0)

class RefSequentialGraphTest(smaug_test.SmaugTest, SequentialGraphTest):
  """Test the sequential graph on the reference backend.

  This test should have no reorder operators because all the inputs are
  already in NCHW format.
  """

  def __init__(self, *args, **kwargs):
    super(RefSequentialGraphTest, self).__init__(*args, **kwargs)
    self.build_test_sequential_graph("Reference")

  def test_parent_children(self):
    """Test the parent/child relationship in the graph."""

    # input (Data).
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(len(node.parents), 0)
    self.assertEqual(node.children[0], "conv0")
    # conv0 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.parents[0], "input")
    self.assertEqual(node.children[0], "conv0_relu")
    # conv0_relu (ReLU).
    node = self.get_node(self.test_graph.graph, "conv0_relu")
    self.assertEqual(node.parents[0], "conv0")
    self.assertEqual(node.children[0], "bn")
    # bn (BN)
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.parents[0], "conv0_relu")
    self.assertEqual(node.children[0], "conv1")
    # conv1 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.parents[0], "bn")
    self.assertEqual(node.children[0], "conv1_relu")
    # conv1_relu (ReLU)
    node = self.get_node(self.test_graph.graph, "conv1_relu")
    self.assertEqual(node.parents[0], "conv1")
    self.assertEqual(node.children[0], "pool")
    # pool (MaxPooling)
    node = self.get_node(self.test_graph.graph, "pool")
    self.assertEqual(node.parents[0], "conv1_relu")
    self.assertEqual(node.children[0], "flatten")
    # flatten (Flatten)
    node = self.get_node(self.test_graph.graph, "flatten")
    self.assertEqual(node.parents[0], "pool")
    self.assertEqual(node.children[0], "fc0")
    # fc0 (FC)
    node = self.get_node(self.test_graph.graph, "fc0")
    self.assertEqual(node.parents[0], "flatten")
    self.assertEqual(node.children[0], "fc0_relu")
    # fc0_relu (ReLU)
    node = self.get_node(self.test_graph.graph, "fc0_relu")
    self.assertEqual(node.parents[0], "fc0")
    self.assertEqual(node.children[0], "fc1")
    # fc1 (FC)
    node = self.get_node(self.test_graph.graph, "fc1")
    self.assertEqual(node.parents[0], "fc0_relu")
    self.assertEqual(len(node.children), 0)

class SMVResidualGraphTest(smaug_test.SmaugTest, ResidualGraphTest):
  """Test the residual graph on the SMV backend."""

  def __init__(self, *args, **kwargs):
    super(SMVResidualGraphTest, self).__init__(*args, **kwargs)
    self.build_test_residual_graph("SMV")

  def test_parent_children(self):
    """Test the parent/child relationship in the graph."""

    # input (Data).
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(len(node.parents), 0)
    self.assertEqual(node.children[0], "input->conv0")
    # Reorder input from NCHW to NHWC.
    node = self.get_node(self.test_graph.graph, "input->conv0")
    self.assertEqual(node.parents[0], "input")
    self.assertEqual(node.children[0], "conv0")
    self.assertEqual(node.children[1], "conv1")
    # conv0 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.parents[0], "input->conv0")
    self.assertEqual(node.children[0], "add")
    # conv1 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.parents[0], "input->conv0")
    self.assertEqual(node.children[0], "conv1->bn")
    # Reorder input from NHWC to NCHW.
    node = self.get_node(self.test_graph.graph, "conv1->bn")
    self.assertEqual(node.parents[0], "conv1")
    self.assertEqual(node.children[0], "bn")
    # bn (BN).
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.parents[0], "conv1->bn")
    self.assertEqual(node.children[0], "relu")
    # relu (ReLU).
    node = self.get_node(self.test_graph.graph, "relu")
    self.assertEqual(node.parents[0], "bn")
    self.assertEqual(node.children[0], "relu->conv2")
    # Reorder input from NCHW to NHWC.
    node = self.get_node(self.test_graph.graph, "relu->conv2")
    self.assertEqual(node.parents[0], "relu")
    self.assertEqual(node.children[0], "conv2")
    # conv2 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv2")
    self.assertEqual(node.parents[0], "relu->conv2")
    self.assertEqual(node.children[0], "add")
    # add (EltwiseAdd).
    node = self.get_node(self.test_graph.graph, "add")
    self.assertEqual(node.parents[0], "conv0")
    self.assertEqual(node.parents[1], "conv2")
    self.assertEqual(len(node.children), 0)

class RefResidualGraphTest(smaug_test.SmaugTest, ResidualGraphTest):
  """Test the residual graph on the reference backend."""

  def __init__(self, *args, **kwargs):
    super(RefResidualGraphTest, self).__init__(*args, **kwargs)
    self.build_test_residual_graph("Reference")

  def test_parent_children(self):
    """Test the parent/child relationship in the graph."""

    # input (Data).
    node = self.get_node(self.test_graph.graph, "input")
    self.assertEqual(len(node.parents), 0)
    self.assertEqual(node.children[0], "conv0")
    self.assertEqual(node.children[1], "conv1")
    # conv0 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv0")
    self.assertEqual(node.parents[0], "input")
    self.assertEqual(node.children[0], "add")
    # conv1 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv1")
    self.assertEqual(node.parents[0], "input")
    self.assertEqual(node.children[0], "bn")
    # bn (BN).
    node = self.get_node(self.test_graph.graph, "bn")
    self.assertEqual(node.parents[0], "conv1")
    self.assertEqual(node.children[0], "relu")
    # relu (ReLU).
    node = self.get_node(self.test_graph.graph, "relu")
    self.assertEqual(node.parents[0], "bn")
    self.assertEqual(node.children[0], "conv2")
    # conv2 (Convolution).
    node = self.get_node(self.test_graph.graph, "conv2")
    self.assertEqual(node.parents[0], "relu")
    self.assertEqual(node.children[0], "add")
    # add (EltwiseAdd).
    node = self.get_node(self.test_graph.graph, "add")
    self.assertEqual(node.parents[0], "conv0")
    self.assertEqual(node.parents[1], "conv2")
    self.assertEqual(len(node.children), 0)

if __name__ == "__main__":
  unittest.main()