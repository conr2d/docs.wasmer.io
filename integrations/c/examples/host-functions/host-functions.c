#include <stdio.h>
#include "wasmer.h"
#include <assert.h>
#include <string.h>

// Function to print the most recent error string from Wasmer if we have them
void print_wasmer_error()
{
  int error_len = wasmer_last_error_length();
  char *error_str = malloc(error_len);
  wasmer_last_error_message(error_str, error_len);
  printf("Error: `%s`\n", error_str);
}

// Global counter our Wasm module will be updating
int counter = 0;

int get_counter(wasmer_instance_context_t *ctx) {
  return counter;
}

int add_to_counter(wasmer_instance_context_t *ctx, int value_to_add) {
  counter += value_to_add;
  return counter;
}

// Function to create a function import to pass to our wasmer instance
wasmer_import_func_t *create_wasmer_import_function(
    void (*function_pointer)(void *),
    wasmer_value_tag params_signature[], 
    int num_params, 
    wasmer_value_tag returns_signature[], 
    int num_returns
    ) {

  // Create a new func to hold the parameter and signature
  // of our `print_str` host function
  wasmer_import_func_t *func = wasmer_import_func_new(
      function_pointer, 
      params_signature, 
      num_params, 
      returns_signature, 
      num_returns
      );

  return func;
}

// Function to create a Wasmer Instance
wasmer_instance_t *create_wasmer_instance(
    wasmer_import_func_t *get_counter_import_function,  
    char *get_counter_import_function_name,
    wasmer_import_func_t *add_to_counter_import_function,
    char *add_to_counter_import_function_name
    ) {

  // Create module name for our imports

  // Create a UTF-8 string as bytes for our module name. 
  // And, place the string into the wasmer_byte_array type so it can be used by our guest Wasm instance.
  const char *module_name = "env";
  wasmer_byte_array module_name_bytes = { .bytes = (const uint8_t *) module_name,
    .bytes_len = strlen(module_name) };

  // Define our get_counter import
  wasmer_byte_array get_counter_import_function_name_bytes = { .bytes = (const uint8_t *) get_counter_import_function_name,
    .bytes_len = strlen(get_counter_import_function_name) };
  wasmer_import_t get_counter_import = { .module_name = module_name_bytes,
    .import_name = get_counter_import_function_name_bytes,
    .tag = WASM_FUNCTION,
    .value.func = get_counter_import_function };

  // Define our add_to_counter import
  wasmer_byte_array add_to_counter_import_function_name_bytes = { .bytes = (const uint8_t *) add_to_counter_import_function_name,
    .bytes_len = strlen(add_to_counter_import_function_name) };
  wasmer_import_t add_to_counter_import = { .module_name = module_name_bytes,
    .import_name = add_to_counter_import_function_name_bytes,
    .tag = WASM_FUNCTION,
    .value.func = add_to_counter_import_function };


  // Define an array containing our imports
  wasmer_import_t imports[] = {get_counter_import, add_to_counter_import};

  // Read the Wasm file bytes
  FILE *file = fopen("../../../shared/c/host-functions.wasm", "r");
  assert(file != NULL);
  fseek(file, 0, SEEK_END);
  long len = ftell(file);
  uint8_t *bytes = malloc(len);
  fseek(file, 0, SEEK_SET);
  fread(bytes, 1, len, file);
  fclose(file);

  // Instantiate a WebAssembly Instance from Wasm bytes and imports
  wasmer_instance_t *instance = NULL;
  wasmer_result_t compile_result = wasmer_instantiate(
      &instance, // Our reference to our Wasm instance 
      bytes, // The bytes of the WebAssembly modules
      len, // The length of the bytes of the WebAssembly module
      imports, // The Imports array the will be used as our importObject
      2 // The number of imports in the imports array
      );

  // Ensure the compilation was successful.
  if (compile_result != WASMER_OK)
  {
    print_wasmer_error();
  }

  // Assert the Wasm instantion completed
  assert(compile_result == WASMER_OK);

  // Return the Wasmer Instance
  return instance;
}

// Function to call a function on the guest Wasm module, and return an i32 result
int call_wasm_function_and_return_i32(wasmer_instance_t *instance, char* functionName, wasmer_value_t params[], int num_params) {
  // Define our results. Results are created with { 0 } to avoid null issues,
  // And will be filled with the proper result after calling the guest Wasm function.
  wasmer_value_t result_one = { 0 };
  wasmer_value_t results[] = {result_one};


  // Call the Wasm function
  wasmer_result_t call_result = wasmer_instance_call(
      instance, // Our Wasm Instance
      functionName, // the name of the exported function we want to call on the guest Wasm module
      params, // Our array of parameters
      num_params, // The number of parameters
      results, // Our array of results
      1 // The number of results
      );

  // Get our response, we know the function is an i32, thus we assign the value to an int
  int response_tag = results[0].tag;
  int response_value = results[0].value.I32; 

  // Return the i32 (int) result.
  return response_value;
}

int main() {

  // Create our function imports

  // Create the get_counter function import
  wasmer_value_tag get_counter_params_sig[] = {};
  wasmer_value_tag get_counter_returns_sig[] = {WASM_I32};
  wasmer_import_func_t *get_counter_import_function = create_wasmer_import_function(
      (void (*)(void *)) get_counter, // Function Pointer
      get_counter_params_sig, // Params Signature
      0, // Number of Params
      get_counter_returns_sig, // Returns Signature
      1 // Number of Returns
      );

  // Create the add_to_counter function
  wasmer_value_tag add_to_counter_params_sig[] = {WASM_I32};
  wasmer_value_tag add_to_counter_returns_sig[] = {WASM_I32};
  wasmer_import_func_t *add_to_counter_import_function = create_wasmer_import_function(
      (void (*)(void *)) add_to_counter, // Function Pointer
      add_to_counter_params_sig, // Params Signature
      1, // Number of Params
      add_to_counter_returns_sig, // Returns Signature
      1 // Number of Returns
      );


  // Initialize our Wasmer Memory and Instance
  wasmer_instance_t *instance = create_wasmer_instance(
      get_counter_import_function,
      "get_counter",
      add_to_counter_import_function,
      "add_to_counter"
      );

  // Set our counter to an initial value
  counter = 24;
  printf("Initial counter value: %d\n", counter);

  // Let's get the pointer to the buffer exposed by our Guest Wasm Module
  // Define our parameters we are passing into the guest Wasm function call.
  // Params are created with the following properties
  // .tag is the tag of the type of the param being passed to the guest Wasm function
  // .value.I32 is the value being passed to the guest Wasm function
  wasmer_value_t increment_counter_loop_param_one = { .tag = WASM_I32, .value.I32 = 10 };
  wasmer_value_t increment_counter_loop_params[] = { increment_counter_loop_param_one };
  int buffer_pointer = call_wasm_function_and_return_i32(instance, "increment_counter_loop", increment_counter_loop_params, 1);

  printf("Final counter value: %d\n", counter);

  assert(counter == 34);

  // Destroy the instances we created for our wasmer
  wasmer_import_func_destroy(get_counter_import_function);
  wasmer_import_func_destroy(add_to_counter_import_function);
  wasmer_instance_destroy(instance);

  return 0;
}