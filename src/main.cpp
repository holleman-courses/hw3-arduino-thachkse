#include <Arduino.h>
// Add additional libraries
#include "model.h"  // Converted TFLite model header
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include <stdint.h>  // For uint8_t



#define INPUT_BUFFER_SIZE 64
#define OUTPUT_BUFFER_SIZE 64
#define INT_ARRAY_SIZE 7

// Namespace to avoid conflicts
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;       // For logging errors
  const tflite::Model* model = nullptr;                  // Points to the loaded model
  tflite::MicroInterpreter* interpreter = nullptr;       // Runs inference
  TfLiteTensor* input = nullptr;                         // Input tensor handle
  TfLiteTensor* output = nullptr;                        // Output tensor handle

  constexpr int kTensorArenaSize = 2 * 1024;             // Memory for model (adjust as needed)
  uint8_t tensor_arena[kTensorArenaSize];                // Tensor arena buffer
}


// put function declarations here:
int string_to_array(char *in_str, int *int_array);
void print_int_array(int *int_array, int array_len);
int sum_array(int *int_array, int array_len);


char received_char = (char)NULL;              
int chars_avail = 0;                    // input present on terminal
char out_str_buff[OUTPUT_BUFFER_SIZE];  // strings to print to terminal
char in_str_buff[INPUT_BUFFER_SIZE];    // stores input from terminal
int input_array[INT_ARRAY_SIZE];        // array of integers input by user

int in_buff_idx=0; // tracks current input location in input buffer
int array_length=0;
int array_sum=0;

// Timing function
unsigned long measure_time_in_us() {
  return micros();  // Returns the current time in microseconds
}

void setup() {
  delay(5000);
  Serial.begin(115200); 
  while (!Serial);                    // Wait until the serial monitor is ready

  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;   // Set up error reporter
  // put your setup code here, to run once:
  
  // Arduino does not have a stdout, so printf does not work easily
  // So to print fixed messages (without variables), use 
  // Serial.println() (appends new-line)  or Serial.print() (no added new-line)
  Serial.println("Test Project waking up");
  memset(in_str_buff, (char)0, INPUT_BUFFER_SIZE*sizeof(char)); 
  // Load the model from the byte array (defined in model.h)
  model = tflite::GetModel(g_sine_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report("Model version mismatch!");
    return;
  }

  // Set up operations resolver (handles the operations your model uses)
  static tflite::AllOpsResolver resolver;

  // Create the interpreter instance
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory for model tensors
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    error_reporter->Report("Tensor allocation failed!");
    return;
  }

  // Get pointers to the model's input and output tensors
  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("✅ TFLM sine prediction model initialized successfully.");
}

void loop() {
  // put your main code here, to run repeatedly:

  // check if characters are avialble on the terminal input
  chars_avail = Serial.available(); 
  if (chars_avail > 0) {
    received_char = Serial.read(); // get the typed character and 
    Serial.print(received_char);   // echo to the terminal

    in_str_buff[in_buff_idx++] = received_char; // add it to the buffer
    if (received_char == 13) { // 13 decimal = newline character
      // user hit 'enter', so we'll process the line.
      Serial.print("About to process line: ");
      Serial.println(in_str_buff);

      // Process and print out the array
      array_length = string_to_array(in_str_buff, input_array);
      sprintf(out_str_buff, "Read in  %d integers: ", array_length);
      Serial.print(out_str_buff);
      print_int_array(input_array, array_length);
      array_sum = sum_array(input_array, array_length);
      sprintf(out_str_buff, "Sums to %d\r\n", array_sum);
      Serial.print(out_str_buff);
      
   //insert array in TFLITE model
      if (array_length == INT_ARRAY_SIZE) {
        // Measure the time to print a statement
        unsigned long t0 = measure_time_in_us();
        Serial.println("Test statement for timing.");
        unsigned long t1 = measure_time_in_us();

        // Pass integers into the model's input tensor
        for (int i = 0; i < INT_ARRAY_SIZE; i++) {
          input->data.int8[i] = static_cast<int8_t>(input_array[i]);
        }

        // Measure and run inference
        unsigned long t2 = 0;
        if (interpreter->Invoke() != kTfLiteOk) {
          Serial.println("Error: Model inference failed.");
        } else {
          t2 = measure_time_in_us();  // Capture time after inference

          // Retrieve and print the model prediction
          int8_t prediction = output->data.int8[0];
          sprintf(out_str_buff, "Predicted next sine value: %d\r\n", prediction);
          Serial.print(out_str_buff);

          // Calculate and print timing results
          unsigned long t_print = t1 - t0;   // Time to print statement
          unsigned long t_infer = t2 - t1;   // Time to run inference

          sprintf(out_str_buff, "Printing time (us): %lu, Inference time (us): %lu\r\n", t_print, t_infer);
          Serial.print(out_str_buff);  
        }
      } else {
        Serial.println("Error: Please enter exactly 7 integers separated by commas.");
      }

      // Clear input buffer and reset index
      memset(in_str_buff, (char)0, INPUT_BUFFER_SIZE * sizeof(char)); 
      in_buff_idx = 0;
    }
    // Prevent input buffer overflow
    else if (in_buff_idx >= INPUT_BUFFER_SIZE) {
      memset(in_str_buff, (char)0, INPUT_BUFFER_SIZE * sizeof(char)); 
      in_buff_idx = 0;
    }    
  }
}

int string_to_array(char *in_str, int *int_array) {
  int num_integers=0;
  char *token = strtok(in_str, ",");
  
  while (token != NULL) {
    int_array[num_integers++] = atoi(token);
    token = strtok(NULL, ",");
    if (num_integers >= INT_ARRAY_SIZE) {
      break;
    }
  }
  
  return num_integers;
}




void print_int_array(int *int_array, int array_len) {
  int curr_pos = 0; // track where in the output buffer we're writing

  sprintf(out_str_buff, "Integers: [");
  curr_pos = strlen(out_str_buff); // so the next write adds to the end
  for(int i=0;i<array_len;i++) {
    // sprintf returns number of char's written. use it to update current position
    curr_pos += sprintf(out_str_buff+curr_pos, "%d, ", int_array[i]);
  }
  sprintf(out_str_buff+curr_pos, "]\r\n");
  Serial.print(out_str_buff);
}

int sum_array(int *int_array, int array_len) {
  int curr_sum = 0; // running sum of the array

  for(int i=0;i<array_len;i++) {
    curr_sum += int_array[i];
  }
  return curr_sum;
}