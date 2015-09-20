define boxcon
add-symbol-file user
target remote localhost:1234
end

define pila
x $esp
x $esp+4
x $esp+8
x $esp+12
x $esp+16
x $esp+20
x $esp+24
x $esp+28
x $esp+32
x $esp+36
x $esp+40
end

