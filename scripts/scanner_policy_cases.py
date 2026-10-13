def tap(delay,x,y,gate='app',release=40):return [(delay,gate,'down',x,y),(release,gate,'up',x,y)]
def nav(delay,kind,gate='app'):return [(delay,gate,kind,0,0)]
def scenarios():
 scan=tap(200,50,218);detail=scan+tap(500,100,80,'scan');name=detail+tap(250,172,218)
 key=tap(250,47,97,'text');done=tap(250,194,188,'text');cancel=tap(250,28,20,'text')
 back=nav(250,'back','text');home=tap(250,210,20,'text')
 result={'idle':[], 'scan-stop':scan+tap(800,50,218,'scan'), 'nested-back':detail+tap(250,28,20)+tap(250,28,20),
 'name-save':name+key+done, 'name-cancel':name+key+cancel, 'name-navigation-back':name+key+back,
 'name-unavailable':name,'name-retained':name+key+cancel,
 'name-back-return':name+key+back+tap(250,28,20)+tap(250,28,20),
 'name-repeat':name+key+done+tap(250,172,218)+key+cancel+tap(250,172,218)+key+done,
 'name-navigation-home':name+key+home,
 'name-save-delayed':name+key+done,
 'name-rapid-repeat':name+tap(10,47,97,'text',release=4)+tap(4,47,97,'text',release=4)+done}
 for boundary in ('acquire','open','poll','close','release','kv-acquire','kv-get','kv-put','kv-release','acquire-fail'):
  result['name-loss-'+boundary]=name+key+done
 return result

