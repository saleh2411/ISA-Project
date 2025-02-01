.word 0x100 1
.word 0x101 2
.word 0x102 3
.word 0x103 4
.word 0x104 -1
.word 0x105 -2
.word 0x106 -3
.word 0x107 -4
.word 0x108 -4
.word 0x109 -3
.word 0x10A -2
.word 0x10B -1
.word 0x10C 4
.word 0x10D 3
.word 0x10E 2
.word 0x10F 1
.word 0x110 2
.word 0x111 2
.word 0x112 2
.word 0x113 2
.word 0x114 0
.word 0x115 1
.word 0x116 0
.word 0x117 1
.word 0x118 -4
.word 0x119 3
.word 0x11A -2
.word 0x11B 1
.word 0x11C 0
.word 0x11D 5
.word 0x11E 6
.word 0x11F 0
add $s0, $zero, $zero, $zero, 0, 0 		# $s0=0, represent row index in matrix 1
add $t2, $zero, $zero, $zero, 0, 0 		# $t2 = number of the corrent element in result matrix

L1:
add $a2, $zero, $zero, $zero, 0, 0		# Initialize $a2 (the column number in matrix 2)

L3:
add $s2, $zero, $zero, $zero, 0, 0		# Initialize $s2
add $s1, $zero, $zero, $zero, 0, 0 		# $s1=0, represent column index in matrix 1

L2:
mac $t0, $s0, $imm1, $s1, 4, 0		    #$t0 = $s0*4+$s1, number of the corrent element in matrix 1
lw $a0, $imm1, $t0, $zero, 0x100, 0	    # Load element from the first matrix to $a0
mac $t1, $s1, $imm1, $a2, 4, 0		    # $t1 = $s1*4+$a2, number of the corrent element in matrix 2
lw $a1, $imm1, $t1, $zero, 0x110, 0	    # Load element from the second matrix to $a1
mac $s2, $a0, $a1, $s2, 0, 0			# $s2 += $a0*$a1
add $s1, $s1, $imm1, $zero, 1, 0		# $s1+= 1 ,move to the next column
bne $zero, $s1, $imm1, $imm2, 4, L2	    # Jump to L2 if column number != 4
# Store result matrix
sw $s2, $t2, $imm1, $zero, 0x120, 0	    # Store element in the result matrix
add $t2, $imm2, $t2, $zero, 0, 1 		# $t2+=1
add $a2, $imm2, $a2, $zero, 0, 1		# Increase $a2 by 1
bne $zero, $a2, $imm1, $imm2, 4, L3	    # Jump to L3 if $a2 != 4
add $s0, $s0, $imm1, $zero, 1, 0		# Move to the next row
bne $zero, $s0, $imm1, $imm2, 4, L1	    # Jump to L1 if row number != 4
halt $zero, $zero, $zero, $zero, 0, 0	# Halt


