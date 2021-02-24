#pip install htmlmin
#or python -m pip install htmlmin
#pip install jsmin
#or python -m pip install jsmin

import gzip
import zlib
import htmlmin
from jsmin import jsmin



content = ""
with open('index.html','rt',encoding="utf-8") as f:
    content=f.read()


content = htmlmin.minify(content, remove_comments=True, remove_empty_space=True, remove_all_empty_space=True, reduce_empty_attributes=True, reduce_boolean_attributes=False, remove_optional_attribute_quotes=True, convert_charrefs=True, keep_pre=False)


import re
regex = r"<script>(.+?)<\/script>"
content = re.sub(regex, lambda x: "<script>"+jsmin(x.group(1))+"</script>" ,content, 0, re.DOTALL)

result =""
for c in zlib.compress(content.encode("UTF-8"),9):
     result= result + ("0x%02X" %c)
     if len(result)> 0:
          result=result + ","


with open('../index_html.h',"wt") as f:
	f.write("const uint8_t index_html[] PROGMEM = {");
	f.write(result.strip(","))
	f.write("};");