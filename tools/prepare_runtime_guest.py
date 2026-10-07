"""Prepare private game C++ for the pinned Android runtime's headers (never modify PC sources)."""
import argparse
from pathlib import Path
import re
import shutil

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--recomp',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
source=args.recomp.resolve()/"port/generated/default"
target=root/".tools/android-guest/generated/default"
shutil.copytree(source,target,dirs_exist_ok=True,ignore=shutil.ignore_patterns('superman_returns_init.cpp'))
path=target/"superman_returns_init.cpp"
text=(source/path.name).read_text(encoding="utf-8")
match=re.search(r"    \.codegen_flags = \{.*?    \},\n",text,re.S)
if match:
    if 'true' in match.group():
        raise SystemExit('Non-default codegen flags require an explicit runtime compatibility review.')
    # All flags are false: the older Android runtime assumes exactly this layout.
    text=text[:match.start()]+text[match.end():]
def write_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text,encoding="utf-8")

write_changed(path,text)
shutil.copy2(args.recomp/"port/src/xma_fixes.cpp",root/".tools/android-guest/xma_fixes.cpp")
intro=(args.recomp/"port/src/skip_intro.cpp").read_text(encoding="utf-8")
intro=intro.replace('#include "sr_settings.h"', '#include <rex/cvar.h>\nREXCVAR_DEFINE_BOOL(sr_skip_intro, true, "Superman", "Skip boot logo/legal sequence");')
write_changed(root/".tools/android-guest/skip_intro.cpp",intro)
print(target)
