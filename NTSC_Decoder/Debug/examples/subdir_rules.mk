################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
examples/color.obj: ../examples/color.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/tms470_4.9.5/bin/cl470" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me -g --include_path="C:/ti/ccsv5/tools/compiler/tms470_4.9.5/include" --include_path="C:/StellarisWare" --include_path="C:/Users/Jarren" --define=ccs="ccs" --define=PART_LM4F120H5QR --define=TARGET_IS_BLIZZARD_RA1 --define=UART_BUFFERED --diag_warning=225 --display_error_number --preproc_with_compile --preproc_dependency="examples/color.pp" --obj_directory="examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

examples/con_ntsc.obj: ../examples/con_ntsc.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/tms470_4.9.5/bin/cl470" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me -g --include_path="C:/ti/ccsv5/tools/compiler/tms470_4.9.5/include" --include_path="C:/StellarisWare" --include_path="C:/Users/Jarren" --define=ccs="ccs" --define=PART_LM4F120H5QR --define=TARGET_IS_BLIZZARD_RA1 --define=UART_BUFFERED --diag_warning=225 --display_error_number --preproc_with_compile --preproc_dependency="examples/con_ntsc.pp" --obj_directory="examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

examples/console.obj: ../examples/console.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/tms470_4.9.5/bin/cl470" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me -g --include_path="C:/ti/ccsv5/tools/compiler/tms470_4.9.5/include" --include_path="C:/StellarisWare" --include_path="C:/Users/Jarren" --define=ccs="ccs" --define=PART_LM4F120H5QR --define=TARGET_IS_BLIZZARD_RA1 --define=UART_BUFFERED --diag_warning=225 --display_error_number --preproc_with_compile --preproc_dependency="examples/console.pp" --obj_directory="examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

examples/draw2.obj: ../examples/draw2.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/tms470_4.9.5/bin/cl470" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me -g --include_path="C:/ti/ccsv5/tools/compiler/tms470_4.9.5/include" --include_path="C:/StellarisWare" --include_path="C:/Users/Jarren" --define=ccs="ccs" --define=PART_LM4F120H5QR --define=TARGET_IS_BLIZZARD_RA1 --define=UART_BUFFERED --diag_warning=225 --display_error_number --preproc_with_compile --preproc_dependency="examples/draw2.pp" --obj_directory="examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

examples/ntsc.obj: ../examples/ntsc.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/tms470_4.9.5/bin/cl470" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me -g --include_path="C:/ti/ccsv5/tools/compiler/tms470_4.9.5/include" --include_path="C:/StellarisWare" --include_path="C:/Users/Jarren" --define=ccs="ccs" --define=PART_LM4F120H5QR --define=TARGET_IS_BLIZZARD_RA1 --define=UART_BUFFERED --diag_warning=225 --display_error_number --preproc_with_compile --preproc_dependency="examples/ntsc.pp" --obj_directory="examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


