	.word 0x100 5
	.word 0x101 2
	sll $sp, $imm1, $imm2, $zero, 1, 11		# set $sp = 1 << 11 = 2048
	lw $a0, $zero, $imm1, $zero, 0x100, 0		# $a0 = n
	lw $a1, $zero, $imm1, $zero, 0x101, 0		# $a1 = k
	jal $ra, $zero, $zero, $imm2, 0, binom		# calc $v0 = binom(n,k)
	sw $zero, $zero, $imm2, $v0, 0, 0x102		# store binom(n,k) in 0x102
	halt $zero, $zero, $zero, $zero, 0, 0			# halt

binom: 
	add $sp, $sp, $imm2, $zero, 0, -4			# adjust stack for 4 items
	sw $zero, $sp, $imm2, $s0, 0, 3			# save $s0
	sw $zero, $sp, $imm2, $ra, 0, 2			# save return address
	sw $zero, $sp, $imm2, $a0, 0, 1			# save n
	sw $zero, $sp, $imm2, $a1, 0, 0			# save k
	beq $zero, $a0, $a1, $imm1, L3,0 			# jump to L3 if n=k
	beq $zero, $zero, $a1, $imm1, L3,0 		# jump to L3 if k=0
	beq $zero, $zero, $zero, $imm2, 0, L1		# jump to L1
	
L3: 
	add $v0, $imm1, $zero, $zero, 1, 0			# n=k || k=0, binom(n,k) = 1
	beq $zero, $zero, $zero, $imm2, 0, L2		# jump to L2

L1: 
	sub $a0, $a0, $imm2, $zero, 0, 1			# calculate n - 1
	jal $ra, $zero, $zero, $imm2, 0, binom		# calc $v0=binom(n-1,k)
	add $s0, $v0, $zero, $zero, 0, 0			# $s0 = binom(n-1,k)
	sub $a1, $a1, $imm2, $zero, 0, 1			# calculate k - 1
	jal $ra, $zero, $zero, $imm2, 0, binom		# calc $v0=binom(n-1,k-1)
	add $v0, $v0, $s0, $zero, 0, 0				# $v0 = binom(n-1,k) + binom(n-1,k-1)
	lw $a1, $sp, $imm2, $zero, 0, 0			# restore $a1
	lw $a0, $sp, $imm2, $zero, 0, 1			# restore $a0
	lw $ra, $sp, $imm2, $zero, 0, 2			# restore $ra
	lw $s0, $sp, $imm2, $zero, 0, 3			# restore $s0

L2:
	add $sp, $sp, $imm2, $zero, 0, 4			# pop 4 items from stack
	beq $zero, $zero, $zero, $ra, 0, 0			# and return
	
