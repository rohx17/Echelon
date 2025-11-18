// ============================================================================
// NeuralNetwork.cpp - Updated with input shape verification
// ============================================================================
#include "NeuralNetwork.h"
#include "happy_model.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// Larger arena for CNN model
const int kArenaSize = 6000;

NeuralNetwork::NeuralNetwork() {
    m_error_reporter = new tflite::MicroErrorReporter();

    m_tensor_arena = (uint8_t *)malloc(kArenaSize);
    if (!m_tensor_arena) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "Could not allocate arena");
        return;
    }

    // Load the voice detection model
    m_model = tflite::GetModel(happy_model);
    if (m_model->version() != TFLITE_SCHEMA_VERSION) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "Model schema mismatch");
        return;
    }
    
    // Add operations needed for CNN
    m_resolver = new tflite::MicroMutableOpResolver<10>();
    m_resolver->AddConv2D();
    m_resolver->AddMaxPool2D();
    m_resolver->AddFullyConnected();
    m_resolver->AddLogistic();  // Sigmoid activation
    m_resolver->AddQuantize();
    m_resolver->AddDequantize();
    m_resolver->AddMean();      // For GlobalAveragePooling2D
    m_resolver->AddReshape();   // May be needed
    
    m_interpreter = new tflite::MicroInterpreter(
        m_model, *m_resolver, m_tensor_arena, kArenaSize, m_error_reporter);

    TfLiteStatus allocate_status = m_interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "AllocateTensors() failed");
        return;
    }

    size_t used_bytes = m_interpreter->arena_used_bytes();
    TF_LITE_REPORT_ERROR(m_error_reporter, "Arena used: %d bytes\n", used_bytes);

    input = m_interpreter->input(0);
    output = m_interpreter->output(0);
    
    // Verify and print input/output shapes
    TF_LITE_REPORT_ERROR(m_error_reporter, "Input shape: ");
    for (int i = 0; i < input->dims->size; i++) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "%d ", input->dims->data[i]);
    }
    TF_LITE_REPORT_ERROR(m_error_reporter, "\n");
    
    // Expected: [1, 79, 10, 1] for new model
    // or [1, 79, 10] for old model
    if (input->dims->size == 4) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "Model expects 4D input (with channel)\n");
    } else if (input->dims->size == 3) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "Model expects 3D input (no channel)\n");
    }
}

NeuralNetwork::~NeuralNetwork() {
    delete m_interpreter;
    delete m_resolver;
    free(m_tensor_arena);
    delete m_error_reporter;
}

float* NeuralNetwork::getInputBuffer() {
    return input->data.f;
}

float NeuralNetwork::predict() {
    TfLiteStatus invoke_status = m_interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(m_error_reporter, "Invoke failed\n");
        return -1.0f;
    }
    return output->data.f[0];
}