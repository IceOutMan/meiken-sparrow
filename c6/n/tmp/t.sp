class A{
    new(){}
    bar(fn){
      fn.call(123)
    }
}
var a = A.new()
a.bar {|n| System.print(n)}