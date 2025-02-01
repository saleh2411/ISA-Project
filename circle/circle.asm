        .word 0x100 40                                 #set radius for example
        lw $s0, $imm1, $zero, $zero, 0x100, 0          #load radius
        mac $s0, $s0, $s0, $zero, 0, 0                 #squere radius   
        add $s1, $zero, $zero, $imm1, -1, 0            #initialize row_counter to -1
WHILE1:
        add $s1, $s1, $imm1, $zero, 1, 0               #row_counter++
        beq $zero, $s1, $imm1, $imm2, 256, EXIT1       #if finished checking all rows - exit
        add $s2, $zero, $zero, $zero, 0, 0             #initialize collumn_counter to 0
WHILE2:
        beq $zero, $s2, $imm1, $imm2, 256, WHILE1      #if finished with a row - go to next row
        sub $t0, $imm1, $s1, $zero, 128, 0             #deltaX for current pixel from the center
        sub $t1, $imm1, $s2, $zero, 128, 0             #deltaY for current pixel from the center
        mac $t0, $t0, $t0, $zero, 0, 0                 #deltaX^2
        mac $t1, $t1, $t1, $zero, 0, 0                 #deltaY^2
        add $t2, $t0, $t1, $zero, 0, 0                 #Pixel radius ^ 2 = deltaX ^ 2 + deltaY ^ 2
        ble $zero, $t2, $s0, $imm1, COLOR, 0           #if pixel radius less or equall to radius - color it
AFTER:
        add $s2, $s2, $imm1, $zero, 1, 0               #collumn_counter++
        beq $zero, $zero, $zero, $imm1, WHILE2, 0      #proceed with next collumn
COLOR:
        mac $t0, $s1, $imm1, $zero, 256, 0              
        add $t0, $t0, $s2, $zero, 0, 0                 #calculate offset
        out $zero, $imm1, $zero, $t0, 20, 0            #set pixel address
        out $zero, $imm1, $zero, $imm2, 21, 255        #set pixel data to 255 = white
        out $zero, $imm1, $zero, $imm2, 22, 1          #set command to write
        beq $zero, $zero, $zero, $imm1, AFTER, 0       #continue
EXIT1:
        halt $zero, $zero, $zero, $zero, 0, 0          #finish simulation when finished scanning pixels