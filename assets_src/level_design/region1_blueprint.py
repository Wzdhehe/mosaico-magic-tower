"""Designer-owned coordinate plans. Deterministic authoring, not a map generator.

Coordinates are zero-based. Only the verification tool exports these plans;
this module never changes main/tower_floors.c or the connected device.
"""
class Plan:
    def __init__(self, number, name, purpose, size=11):
        self.number, self.name, self.purpose = number, name, purpose
        self.grid = [list('#' * size) for _ in range(size)]
    def room(self, x1, y1, x2, y2):
        for y in range(y1, y2 + 1):
            for x in range(x1, x2 + 1):
                self.grid[y][x] = '.'
        return self
    def line(self, x1, y1, x2, y2):
        assert x1 == x2 or y1 == y2
        return self.room(min(x1,x2), min(y1,y2), max(x1,x2), max(y1,y2))
    def put(self, **tiles):
        for tile, positions in tiles.items():
            for x,y in positions:
                assert self.grid[y][x] == '.', (self.number,tile,x,y)
                self.grid[y][x] = tile
        return self
    @property
    def rows(self):
        return [''.join(row) for row in self.grid]
    def data(self):
        return dict(floor=self.number, name=self.name, purpose=self.purpose, rows=self.rows)

FLOORS = []
p = Plan(1, '破门', '教学主轴与暂留骑士库')
p.room(3,8,7,9).line(5,4,5,8).room(3,2,7,4)
p.line(7,2,9,2).line(9,1,9,2).line(1,2,1,4).line(1,3,3,3)
p.put(D=[(5,9)], y=[(7,9)], s=[(5,7)], Y=[(5,6)], a=[(5,3),(1,2)],
      d=[(7,4)], p=[(3,4)], P=[(1,4)], k=[(2,3)], v=[(8,2)], U=[(9,1)])
FLOORS.append(p)

p = Plan(2, '偏院', '产钥匙层；实际黄门隔开两种小库')
p.line(1,1,1,9).line(1,1,7,1).room(1,7,3,9).line(3,5,3,7).line(1,5,3,5)
p.room(1,1,3,5)
p.room(2,2,2,2)
p.line(1,3,9,3).line(9,3,9,4).line(3,8,9,8).line(9,7,9,9)
p.put(D=[(1,9)], U=[(7,1)], y=[(3,8),(2,2)], v=[(4,1),(8,8)],
      Y=[(6,3),(6,8)], s=[(8,3)], d=[(9,3)], a=[(9,8)],
      p=[(9,4),(9,7),(9,9),(1,4)])
FLOORS.append(p)

p = Plan(3, '旧库', '可见蓝门防御库；血量通道与黄门通道')
p.line(1,9,9,9).line(1,1,1,9).line(5,2,5,9).line(1,3,5,3)
p.room(1,7,3,9)
p.line(5,2,7,2).room(7,1,9,3)
p.put(D=[(9,9)], U=[(1,1)], B=[(6,2)], d=[(7,1),(9,1)], r=[(9,3)],
      v=[(1,4)], Y=[(5,5)], p=[(1,2),(8,2)], n=[(3,9)])
FLOORS.append(p)

p = Plan(4, '哨楼', '首把蓝钥匙；折返下楼与继续上楼的决策点')
p.line(1,1,9,1).line(7,1,7,3).line(3,3,9,3)
p.room(3,1,5,3).room(7,7,9,9)
p.line(3,3,3,7).line(3,7,9,7).line(9,3,9,9).line(1,1,1,2)
p.put(D=[(1,1)], U=[(9,9)], v=[(8,1)], b=[(9,1)], Y=[(3,5)],
      k=[(9,5)], d=[(5,3)], p=[(3,7)], y=[(9,8)], n=[(1,2)])
FLOORS.append(p)

p = Plan(5, '军械库', '蓝门双攻宝物；骑士旁库可留待回收')
p.line(5,1,5,9).line(1,1,5,1).line(5,7,7,7).room(7,6,9,8)
p.room(3,7,5,9)
p.line(3,5,5,5).room(1,4,3,6)
p.put(D=[(5,9)], U=[(1,1)], v=[(5,5)], B=[(6,7)], a=[(9,6),(9,8)],
      p=[(8,7),(5,2),(1,6)], k=[(4,5)], y=[(1,4)])
FLOORS.append(p)

p = Plan(6, '军需站', '安全停顿；商店选择而非每层战斗')
p.line(1,5,9,5).room(3,3,7,7).line(5,1,5,3).line(3,1,7,1)
p.put(D=[(1,5)], U=[(9,5)], S=[(5,3)], n=[(5,7)], y=[(3,7)],
      Y=[(5,2)], p=[(3,1),(7,1),(7,7)])
FLOORS.append(p)

p = Plan(7, '守望台', '第二把蓝钥匙；钥匙换路或支付战斗血量')
p.line(5,1,9,1).line(5,1,5,7).line(1,7,9,7).line(1,7,1,9)
p.room(5,1,9,3).room(1,7,3,9)
p.line(1,3,5,3).line(1,3,1,7).line(9,7,9,8)
p.put(D=[(9,1)], U=[(1,9)], v=[(5,5)], b=[(5,6)], Y=[(3,3)],
      k=[(8,7)], P=[(9,7)], d=[(9,8)])
FLOORS.append(p)

p = Plan(8, '旧营房', '短路强敌与黄门绕路；西南可选藏室')
p.line(1,1,9,1).line(1,3,9,3).line(1,1,1,3).line(9,1,9,3)
p.room(1,1,3,3).room(7,1,9,3)
p.line(3,3,3,6).line(2,6,3,6).line(2,6,2,8).room(1,8,3,9)
p.put(D=[(1,1)], U=[(9,1)], k=[(5,1)], Y=[(5,3)], v=[(7,3)], n=[(3,6)],
      P=[(2,9)], y=[(1,9)])
p.grid[7][2] = '%'
FLOORS.append(p)

p = Plan(9, '封门前厅', '红钥匙兑现；通往首领前可以回收旧物')
p.line(1,9,9,9).line(9,1,9,9).line(1,1,1,9).room(1,1,5,3)
p.room(1,7,3,9)
p.line(1,7,5,7).line(5,5,5,7).line(5,5,7,5)
p.put(D=[(1,9)], U=[(9,1)], R=[(9,3)], p=[(9,6),(3,2)], y=[(2,9)],
      k=[(1,5)], a=[(5,1)], Y=[(5,6)], P=[(7,5)])
FLOORS.append(p)

p = Plan(10, '队长之门', '首领→章节完成门→11层；没有提前全塔胜利')
p.line(5,1,5,5).room(1,5,9,9).line(3,1,7,1)
for x,y in [(3,6),(7,6),(3,8),(7,8)]:p.grid[y][x]='#'
p.put(D=[(5,9)], U=[(5,1)], k=[(5,3)], G=[(5,2)], p=[(2,8)], n=[(8,8)])
FLOORS.append(p)
