
fun mkFn() {
   System.print("number of map:")
   var myMap = Map.new()
   myMap["xh"] = "rd"
   myMap["xm"] = "op"
   myMap["lw"] = "manager"
   myMap["lz"] = "pm"

   System.gc()  //可以手动回收内存

   for k (myMap.keys) {
      System.print(k + " -> " + myMap[k])
   }
}
var thread = Thread.new(mkFn)
thread.call()
