begin = os.clock();
str = "Hello! My name is Jack. What is your name?"
i = 0
while i <= 500 do
    string.find(str,"Jack")
    i = i + 1
end
 
print('The program use ', os.clock() - begin, 's')

function test(s, n)
    print(s)
    print(n)
end