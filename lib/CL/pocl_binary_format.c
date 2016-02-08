#include "pocl_binary_format.h"

int poclcc_check_global(poclcc_global *binary_format){
  if (binary_format->version != POCLCC_VERSION)
    return 0;
  return !strncmp(binary_format->pocl_id, POCLCC_STRING_ID, POCLCC_STRING_ID_LENGTH);
}

int poclcc_check_device(poclcc_device *device){
  return !strncmp(device->pocl_id, POCLCC_STRING_ID, POCLCC_STRING_ID_LENGTH);
}

int poclcc_get_device_id(cl_device_id device){
  return device->vendor_id;
}

int poclcc_check_device_id(cl_device_id device, poclcc_device *devicecc){
  return poclcc_get_device_id(device) == devicecc->device_id;
}

int poclcc_check_binary(cl_device_id device, const unsigned char *binary){
  poclcc_device *devicecc = (poclcc_device *)binary;
  return poclcc_check_device(devicecc) && poclcc_check_device_id(device, devicecc);
}




int sizeofPoclccKernel(poclcc_kernel *kernel){
  return sizeof(poclcc_kernel) + kernel->sizeofKernelName + kernel->sizeofBinary
    -sizeof(kernel->kernel_name) - sizeof(kernel->binary);
}

int sizeofPoclccDevice(poclcc_device *device){
  int size = sizeof(poclcc_device) - sizeof(device->kernels);
  int i;
  for (i=0; i<device->num_kernels; i++){
    size += sizeofPoclccKernel(&(device->kernels[i]));
  }
}

int sizeofPoclccGlobal(poclcc_global *binary_format){
  int size = 0;
  if (binary_format == NULL)
    return -1;
    
  size += sizeof(poclcc_global) - sizeof(poclcc_device *);
  int i;
  for (i=0; i<binary_format->num_devices; i++)
    size += sizeofPoclccDevice(&(binary_format->devices[i]));
  
  return size;
}

int sizeofPoclccGlobalFromBinariesSizes(size_t *binaries_sizes, int num_devices){
  if (binaries_sizes == NULL)
    return -1;

  int i;
  int size = 0;
  for (i=0; i<num_devices; i++){
    size += binaries_sizes[i];
  }
  size += sizeof(poclcc_global) - sizeof(poclcc_device *);
  return size;
}




cl_int programInfos2BinaryFormat(poclcc_global *binary_format, unsigned char **binaries,
                                 unsigned num_devices){
  strncpy(binary_format->pocl_id, POCLCC_STRING_ID, POCLCC_STRING_ID_LENGTH);
  binary_format->version = POCLCC_VERSION;
  binary_format->num_devices = num_devices;
  if ((binary_format->devices = malloc(num_devices*sizeof(poclcc_device))) == NULL){
    return CL_OUT_OF_HOST_MEMORY;
  }

  int i;
  for (i=0; i<num_devices; i++){
    poclcc_device *device_dst = &(binary_format->devices[i]);
    poclcc_device *device_src = (poclcc_device *)(binaries[i]);
    memcpy(device_dst, device_src, sizeof(poclcc_device));    
    assert(poclcc_check_device(device_dst));
  }
  return CL_SUCCESS;
}

cl_int binaryFormat2ProgramInfos(unsigned char ***binaries, size_t **binaries_sizes, 
                                 poclcc_global *binary_format){
  assert(poclcc_check_global(binary_format));
  int num_devices = binary_format->num_devices;

  if ((*binaries_sizes = malloc(sizeof(size_t)*num_devices)) == NULL)
    goto ERROR;
  
  if ((*binaries = malloc(sizeof(unsigned char*)*num_devices)) == NULL)
    goto ERROR_CLEAN_BINARIES_SIZES;
  
  int i;
  for (i=0; i<num_devices; i++){
    poclcc_device *device = &(binary_format->devices[i]);
    (*binaries)[i] = (unsigned char*)device;
    (*binaries_sizes)[i] = sizeofPoclccDevice(device);
  }
  return CL_SUCCESS;

ERROR_CLEAN_BINARIES_SIZES:
  free(binaries_sizes);
ERROR:
  return CL_OUT_OF_HOST_MEMORY;
}




int binaryFormat2Buffer(char *buffer, int sizeofBuffer, poclcc_global *binary_format){
  char *endofBuffer = buffer + sizeofBuffer;

  assert(poclcc_check_global(binary_format));
  memcpy(buffer, binary_format, sizeof(poclcc_global));
  buffer = (char *)(&(((poclcc_global *)buffer)->devices));
  assert(buffer < endofBuffer && "buffer is not a binaryformat");

  int i;
  for (i=0; i<binary_format->num_devices; i++){
    poclcc_device *device = &(binary_format->devices[i]);
    assert(poclcc_check_device(device));
    memcpy(buffer, device, sizeof(poclcc_device));
    buffer = (char*)(&(((poclcc_device *)buffer)->kernels));
    assert(buffer < endofBuffer && "buffer is not a binaryformat");

    int j;
    for (j=0; j<device->num_kernels; j++){
      poclcc_kernel *kernel = &(device->kernels[j]);

      *((uint32_t *)buffer) = kernel->sizeofKernelName;
      buffer += sizeof(uint32_t);
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

      memcpy(buffer, kernel->kernel_name, kernel->sizeofKernelName);
      buffer += kernel->sizeofKernelName;
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

      *((uint32_t *)buffer) = kernel->sizeofBinary;
      buffer += sizeof(uint32_t);
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

      memcpy(buffer, kernel->binary, kernel->sizeofBinary);
      buffer += kernel->sizeofBinary;      
      assert(buffer < endofBuffer && "buffer is not a binaryformat");
    }
  }
  return sizeofBuffer - (int)(endofBuffer - buffer) == 0? 
    CL_SUCCESS:
    sizeofBuffer - (int)(endofBuffer - buffer);  
}

int buffer2BinaryFormat(poclcc_global *binary_format, char *buffer, int sizeofBuffer){
  char *endofBuffer = buffer + sizeofBuffer;

  memcpy(binary_format, buffer, sizeof(poclcc_global));
  assert(poclcc_check_global(binary_format) && "check file identifier and version");
  buffer = (char *)(&(((poclcc_global *)buffer)->devices));
  assert(buffer < endofBuffer && "buffer is not a binaryformat");
  
  if ((binary_format->devices = malloc(binary_format->num_devices*sizeof(poclcc_device))) == NULL)
    goto ERROR;

  int i;
  for (i=0; i<binary_format->num_devices; i++){
    poclcc_device *device = &(binary_format->devices[i]);
    memcpy(device, buffer, sizeof(poclcc_device));
    buffer = (char *)&(((poclcc_device *)buffer)->kernels);
    assert(buffer < endofBuffer && "buffer is not a binaryformat");
    assert(poclcc_check_device(device));

    if ((device->kernels = malloc(device->num_kernels*sizeof(poclcc_kernel))) == NULL)
      goto ERROR_CLEAN_DEVICE;
   
    int j;
    for (j=0; j<device->num_kernels; j++){
      poclcc_kernel *kernel = &(device->kernels[j]);

      kernel->sizeofKernelName = *((uint32_t *)buffer);
      buffer += sizeof(uint32_t);
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

      if ((kernel->kernel_name = malloc(kernel->sizeofKernelName*sizeof(char))) == NULL)
        goto ERROR_CLEAN_DEVICE_KERNEL;

      memcpy(kernel->kernel_name, buffer, kernel->sizeofKernelName);
      buffer += kernel->sizeofKernelName;
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

      kernel->sizeofBinary = *((uint32_t *)buffer);
      buffer += sizeof(uint32_t);
      assert(buffer < endofBuffer && "buffer is not a binaryformat");
      
      if ((kernel->binary = malloc(kernel->sizeofBinary*sizeof(char))) == NULL)
        goto ERROR_CLEAN_DEVICE_KERNEL;

      memcpy(kernel->binary, buffer, kernel->sizeofBinary);
      buffer += kernel->sizeofBinary;      
      assert(buffer < endofBuffer && "buffer is not a binaryformat");

    }
  }
  return sizeofBuffer - (int)(endofBuffer - buffer) == 0? 
    CL_SUCCESS:
    sizeofBuffer - (int)(endofBuffer - buffer);  

ERROR_CLEAN_DEVICE_KERNEL:
  for (i=0; i<binary_format->num_devices; i++){
    poclcc_device *device = &(binary_format->devices[i]);
    int j;
    for (j=0; j<device->num_kernels; j++){
      poclcc_kernel *kernel = &(device->kernels[j]);
      free(kernel->kernel_name);
      free(kernel->binary);
    }
    free(device->kernels);
  }
ERROR_CLEAN_DEVICE:
  free(binary_format->devices);
ERROR:
  return CL_OUT_OF_HOST_MEMORY;
}



void poclcc_init_device(poclcc_device *devicecc, cl_device_id device, 
                        int num_kernels, poclcc_kernel *kernels){
  strncpy(devicecc->pocl_id, POCLCC_STRING_ID, POCLCC_STRING_ID_LENGTH);
  devicecc->device_id = poclcc_get_device_id(device);
  devicecc->num_kernels = num_kernels;
  devicecc->kernels = kernels;
}

void poclcc_init_kernel(poclcc_kernel *kernelcc, char *kernel_name, int sizeofKernelName, 
                        unsigned char *binary, int sizeofBinary){
  kernelcc->sizeofKernelName = sizeofKernelName;
  kernelcc->kernel_name = kernel_name;
  kernelcc->sizeofBinary = sizeofBinary;
  kernelcc->binary = binary;
}

int LookForKernelBinary(poclcc_global *binary_format, cl_device_id device, char *kernel_name, 
                        char **binary, int *binary_size){
  int i;
  for (i=0; i<binary_format->num_devices; i++){
    poclcc_device *devicecc = (poclcc_device *)(&(binary_format->devices[i]));
    if (poclcc_check_device(devicecc) && poclcc_check_device_id(device, devicecc)){
      int j;
      for (j=0; j<devicecc->num_kernels; j++){
        poclcc_kernel *kernel = &(devicecc->kernels[i]);
        if (!strncmp(kernel->kernel_name, kernel_name, kernel->sizeofKernelName)){
          *binary = kernel->binary;
          *binary_size = kernel->sizeofBinary;
          return CL_SUCCESS;
        }
      }
    }
  }
  return CL_INVALID_PROGRAM_EXECUTABLE;
}
