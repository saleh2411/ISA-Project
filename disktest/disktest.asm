    add $s0, $zero, $imm1, $zero, 8, 0         # initialize s0 to 8
LOOP:
    sub $s0, $s0, $imm1, $zero, 1, 0           # s0--
    jal $ra, $zero, $zero, $imm1, WAIT, 0      # go to waiting routine which ends only when disk is ready
    out $zero, $imm1, $zero, $s0, 15, 0        # disksector = s0 (to read)
    out $zero, $imm1, $zero, $zero, 16, 0      # diskbuffer = 0 (to read)
    out $zero, $imm1, $zero, $imm2, 14, 1      # diskcmd = 1 (read)
    jal $ra, $zero, $zero, $imm1, WAIT, 0      # wait until finish reading from disk
    add $s1, $s0, $zero, $imm1, 1, 0           # s1 = s0 + 1 (each sector is written to one above it)
    out $zero, $imm1, $zero, $s1, 15, 0        # disksector = s1 (to write)
    out $zero, $imm1, $zero, $zero, 16, 0      # diskbuffer = 0 (to write)
    out $zero, $imm1, $zero, $imm2, 14, 2      # diskcmd = 2 (write)
    bne $zero, $s0, $zero, $imm1, LOOP, 0      # if s0 != 0, do again with s0--, that way we copy 7 to 8, then 6 to 7 ... then 0 to 1 and continue
    jal $ra, $zero, $zero, $imm1, WAIT, 0      # wait for the writing to sector 1 to finish
    add $t0, $zero, $zero, $zero, 0 , 0        # initialize t0 = 0
LOOP2:
    sw $zero, $t0, $zero, $zero, 0, 0          # The following 3 lines just itterate over 128 memory first lines and set it to 0
    add $t0, $t0, $zero, $imm1, 1, 0
    bne $zero, $t0, $imm1, $imm2, 128, LOOP2
    out $zero, $imm1, $zero, $zero, 15, 0      # The following 3 line write to sector 0 the 128 first lines of memory (zeros) so sector 0 will be empty
    out $zero, $imm1, $zero, $zero, 16, 0
    out $zero, $imm1, $zero, $imm2, 14, 2
    jal $ra, $zero, $zero, $imm1, WAIT, 0      # wait until writing to sector 0 is done
    halt $zero, $zero, $zero, $zero, 0, 0      # finish simulation
WAIT:
    in $t0, $imm1, $zero, $zero, 17, 0         # t0 = diskstatus
    bne $zero, $t0, $zero, $imm2, 0, WAIT      # if status indicates not ready, check again (will happen for 1024 cycles max)
    beq $zero, $zero, $zero, $ra, 0, 0         # if status indicates ready, continue to use disk