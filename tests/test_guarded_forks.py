"""Complete alternative-strategy replays plus local topology checks."""
import importlib.util
from pathlib import Path
from collections import deque
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("route",ROOT/"tools"/"magic_tower_route.py")
route=importlib.util.module_from_spec(spec)
spec.loader.exec_module(route)

def area(grid,blocked,start):
    queue=deque([start])
    seen={start}
    while queue:
        x,y=queue.popleft()
        for dx,dy in route.MOVE.values():
            nx,ny=x+dx,y+dy
            if (0<=nx<11 and 0<=ny<11 and (nx,ny) not in seen
                    and grid[ny][nx] not in "#"+blocked):
                seen.add((nx,ny))
                queue.append((nx,ny))
    return seen

class GuardedForkTests(unittest.TestCase):
    def test_6f(self):
        f=route.load_maps()[5]
        start=next((x,y) for y,row in enumerate(f) for x,c in enumerate(row) if c=="D")
        # The one yellow key sits between the mage and the golem: either
        # guardian can be fought, but neither can be bypassed.
        self.assertNotIn((5,8),area(f,"mg",start))
        self.assertIn((5,8),area(f,"m",start))
        self.assertIn((5,8),area(f,"g",start))

    def test_9f(self):
        f=route.load_maps()[8]
        start=next((x,y) for y,row in enumerate(f) for x,c in enumerate(row) if c=="D")
        # The exit itself is behind the wraith; the golem is an optional
        # reward guard on the open terrace.
        self.assertNotIn((1,9),area(f,"w",start))
        self.assertIn((1,9),area(f,"g",start))
        self.assertIn((1,9),area(f,"",start))

    def test_four_complete_strategy_replays(self):
        outcomes=[]
        for first in ("m","g"):
            for second in ("g","w"):
                game=route.plan_variant({6:first,9:second})
                result=game.summary()
                self.assertTrue(result["cleared"],(first,second))
                self.assertGreater(result["hp"],0,(first,second))
                self.assertEqual(result["floor"],9)
                outcomes.append((result["hp"],result["attack"],result["defence"]))
        self.assertGreaterEqual(len(set(outcomes)),2)
if __name__=="__main__":
    unittest.main()
