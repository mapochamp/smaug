#include "catch.hpp"
#include "smaug/core/backend.h"
#include "smaug/core/tensor.h"
#include "smaug/core/smaug_test.h"
#include "smaug/operators/my_custom_operator.h"
 
using namespace smaug;
 
TEST_CASE_METHOD(SmaugTest, "my custom operator", "[ops]") {
  // DataLayout::NC is a simple 2D layout, where N = batches and C = a column
  // of data.
  TensorShape shape({1, 10}, DataLayout::NC);
  Tensor* input0 = new Tensor("tensor0", shape);
  // Allocate the memory for a 1x10 array of floats.
  input0->allocateStorage<float>();
  // Add some testing data.
  input0->fillData<float>({1,2,3,4,5,6,7,8,9,10});
  workspace()->addTensor(input0);
 
  // Repeat this for a second input tensor.
  Tensor* input1 = new Tensor("tensor1", shape);
  input1->allocateStorage<float>();
  input1->fillData<float>({2,3,4,5,6,7,8,9,10,11});
  workspace()->addTensor(input1);
 
  // Create the operator and fill it with our tensors.
  using TestOp = MyCustomOperator<ReferenceBackend>;
  //auto op = new TestOp("eltwise_add", workspace());
  auto op = new MyCustomOperator<ReferenceBackend>("MyCustom", workspace());
  op->setInput(input0, TestOp::kInput0);
  op->setInput(input1, TestOp::kInput1);
  op->createAllTensors();
  // Allocates memory for all the output tensors created by createAllTensors.
  allocateAllTensors<float>(op);
 
  op->run();
 
  // Compare the output of the operator against expected values.
  std::vector<float> expected_output = {3,5,7,9,11,13,15,17,19,21};
  Tensor* output = op->getOutput(TestOp::kOutput);
  // This performs an approximate comparison between the tensor's output and
  // the expected values.
  verifyOutputs(output, expected_output);
}

// A function to fill the tensor with a sequence of monotonically increasing
// data, starting from 0. Note that this is ONLY advised for elementwise/unary
// operators in which we don't care about data in specific dimensions.
void fillTensorWithSequentialFloat32Data(Tensor* tensor) {
  //float32* data = tensor->data<float>();
  float* data = tensor->data<float>();
  //for (int i = 0; i < tensor->getShape().getStorageSize(); i++) {
  for (int i = 0; i < tensor->getShape().storageSize(); i++) {
    data[i] = i;
  }
}
 
TEST_CASE_METHOD(SmaugTest, "my custom with tiling", "[tiling]") {
  // With float32 elements, this will occupy 128KB, which should create four
  // tiles per tensor.
  ReferenceBackend::initGlobals();
  TensorShape shape({8, 4096}, DataLayout::NC);
  Tensor* input0 = new Tensor("tensor0", shape);
  Tensor* input1 = new Tensor("tensor1", shape);
  input0->allocateStorage<float>();
  input1->allocateStorage<float>();
  workspace()->addTensor(input0);
  workspace()->addTensor(input1);
 
  // Create the operator and fill it with our tensors.
  using TestOp = MyCustomOperator<ReferenceBackend>;
  auto op = new TestOp("MyCustomTiled", workspace());
  op->setInput(input0, TestOp::kInput0);
  op->setInput(input1, TestOp::kInput1);
  // This will handle creating/allocating storage/filling data into all the
  // input tensors.
  createAndFillTensorsWithData<float>(op, &fillTensorWithSequentialFloat32Data);
  // Compute the expected output.
  std::vector<float> expected_output(8*4096, 0);
  for (int i = 0; i < expected_output.size(); i++) {
    expected_output[i] = 2*i;
  }
 
  op->tile();
  op->run_tiled();
 
  using TestOp = MyCustomOperator<ReferenceBackend>;
  Tensor* output = op->getOutput(TestOp::kOutput);
  verifyOutputs(output, expected_output);
  ReferenceBackend::freeGlobals();
}

