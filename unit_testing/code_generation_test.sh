#! /bin/bash

# This script checks that a project has initialised the ADC and DMA correctly
# both in the .ioc and main file by checking the auto-generated code from STM. 
# If the .ioc-file or the main file initialises them in reverse order the
# ADC will not start, in which case the script fails. 

# INPUTS:
# path:         The script takes one input parameter which should be the path 
#               to the top layer of the project of interest.
#
# OUTPUT:
# exit value:   If the test is successfull the exit value is 0 otherwise 1.

# Get the project path 
path=$1
if [[ ! -d "../STM32/$path" ]]; then
    echo "Error: Missing project path."
    exit 1
fi

# Step 1: Ensure correct initialisation in .ioc-file.
# Check for project .ioc file
project_ioc=$(find ../STM32/$path/ -iname *.ioc)

# Find function sort list
function_list=$(cat $project_ioc | grep ProjectManager.functionlistsort)
function_list_line_number=$(cat $project_ioc | grep -n ProjectManager.functionlistsort | cut -d : -f 1)

# Find indices for MX_ADC and MX_DMA
adc_idx=$(echo "$function_list" | grep -oE '([0-9]+)-MX_ADC' | awk -F'-' '{print $1}')
dma_idx=$(echo "$function_list" | grep -oE '([0-9]+)-MX_DMA' | awk -F'-' '{print $1}')

# If DMA or ADC is not used exit script.
if [[ -z "$adc_idx" || -z "$dma_idx" ]]; then
    exit 0
fi

# Ensure dma_idx is lower than adc_idx
for idx in $adc_idx; do
    if [[ $idx -lt $dma_idx ]]; then
        echo "MX_DMA needs to before initialised before MX_ADC in line $function_list_line_number in $project_ioc."
        exit 1
    fi
done

## Step 2: Ensure correct initialisation in main file.
project_main=$(find ../STM32/$path/Core/Src/ -iname main.c)

# Find lines where MX_ADCx_Init() and MX_DMA_Init() are initialised in main.c
adc_main_idx=$(cat $project_main | grep -n 'MX_ADC[0-9]_Init()'| cut -d : -f 1)
dma_main_idx=$(cat $project_main | grep -n 'MX_DMA_Init()' | cut -d : -f 1)

# Ensure dma_main_idx is lower than adc_main_idx
# NOTE: No need to check if ADC or DMA is used in project as
#       this has already been checked in previous step.
for idx in $adc_main_idx; do
    if [[ $idx -lt $dma_main_idx ]]; then
        echo "MX_DMA_Init() needs to before initialised before MX_ADCx_Init() in $project_main."
        echo "MX_DMA_Init() is initialised in line $dma_main_idx and MX_ADCx_Init() is initialised in line $adc_main_idx."
        exit 1
    fi
done
echo "Static analysis passed."
exit 0