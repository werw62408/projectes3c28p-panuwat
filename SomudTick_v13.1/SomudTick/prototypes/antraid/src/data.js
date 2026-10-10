"use strict";
// ===== Game data: ants (cards), enemies, bases, buildings, boosts, perks, levels =====
// Stats follow the original game's wiki tables where they exist (level 1..5).
// Units: speed x SPD_PX = px per second, vision x VIS_PX = px, hatch x HATCH_K = seconds.

const SCR_W = 240, SCR_H = 320;          // the board's screen, portrait
const TOP_H = 14, BAR_H = 28;            // top bar, bottom button bar
const VIEW_Y = TOP_H, VIEW_H = SCR_H - TOP_H - BAR_H;
const T = 8;                             // tile size in px
const SURF_W = 48, SURF_H = 64;          // surface map in tiles (384 x 512 px)
const UND_W = 30, UND_H = 40;            // underground grid in tiles (240 x 320 px)
const SPD_PX = 2.8, VIS_PX = 5, HATCH_K = 0.5, FOOD_K = 2;   // FOOD_K: food per carry point (keeps the original prices with our shorter map)
const RAR = ["Common", "Rare", "Epic", "Legendary"];
const RAR_COL = ["#9aa3ad", "#4f9bff", "#b065ff", "#ffb02e"];
const CASTES = {
  queen: { name: "Queen", col: "#ffcf4a" },
  builder: { name: "Builder", col: "#d9a066" },
  loader: { name: "Loader", col: "#7fd46b" },
  scout: { name: "Scout", col: "#5ec8ff" },
  warrior: { name: "Warrior", col: "#ff5d4a" },
  guardian: { name: "Guardian", col: "#c08bff" },
  shooter: { name: "Shooter", col: "#ff9a3c" },
};
// deck must hold these castes (guardian and shooter are optional)
const NEED_CASTES = ["queen", "builder", "loader", "scout", "warrior"];

// L = per-level arrays (index 0 = level 1). A plain number = same at every level.
// rb = stats raised by card rarity (+10/+20/+30 %), price also -10/-20 % at Epic/Legendary.
// leg = extra stat at Legendary.
const ANTS = {
  uqueen: { name: "Universal Queen", short: "U.QUEEN", castes: ["queen"], size: 2.2,
    hp: 70, dmg: 2, as: 1, cd: 3, cc: 0, spd: 1.5, str: 0, vis: 4, dist: 0, price: 0, hatch: 0,
    q: { qsCost: 12, qsFood: 4, qsEgg: 1, startQS: 6, startFS: 0, startBR: 0, eggs: 5 },
    skills: ["Lays eggs from food", "Queen rooms keep eggs and food"] },
  fqueen: { name: "Queen of Fields", short: "F.QUEEN", castes: ["queen"], size: 2.2,
    hp: 70, dmg: 2, as: 1, cd: 3, cc: 0, spd: 1.5, str: 0, vis: 4, dist: 0, price: 0, hatch: 0,
    q: { qsCost: 15, qsFood: 0, qsEgg: 2, startQS: 4, startFS: 2, startBR: 0, eggs: 5, fieldBonus: 0.1 },
    skills: ["Queen rooms keep 2 eggs (no food)", "Food sources +10 %"] },
  squeen: { name: "Skirmish Queen", short: "S.QUEEN", castes: ["queen"], size: 2.2,
    hp: 70, dmg: 2, as: 1, cd: 3, cc: 0, spd: 1.5, str: 0, vis: 4, dist: 0, price: 0, hatch: 0,
    q: { qsCost: 12, qsFood: 4, qsEgg: 1, startQS: 4, startFS: 0, startBR: 2, eggs: 4 },
    skills: ["Starts with 2 barracks built", "Queen rooms keep eggs and food"] },
  builder: { name: "Builder", short: "BUILDER", castes: ["builder"], size: 1.0, under: true,
    hp: [10, 12, 14, 16, 20], dmg: [1, 1, 1, 2, 2], as: 2, cd: 2, cc: 0, spd: [8, 9, 10, 11, 11], str: [1, 1, 2, 2, 3],
    vis: 3, dist: 1, price: [15, 20, 25, 30, 35], hatch: 68, rb: ["spd", "hp"], leg: ["str", 1],
    skills: ["Digs, fills and builds", "Feeds the queen", "Carries chitin to the workshop"] },
  loader: { name: "Loader", short: "LOADER", castes: ["loader"], size: 1.1,
    hp: [7, 10, 12, 14, 16], dmg: [1, 1, 2, 2, 2], as: 1, cd: 2, cc: 0, spd: 6, str: [1, 2, 2, 2, 3],
    vis: 6, dist: 2, price: [7, 12, 18, 24, 30], hatch: 64, rb: ["spd", "dist"], leg: ["str", 1],
    skills: ["Carries food home", "Short patrol: fights near the nest"] },
  hopper: { name: "Hopper", short: "HOPPER", castes: ["loader"], size: 1.1,
    hp: [6, 8, 10, 12, 14], dmg: 1, as: 1, cd: 2, cc: 0, spd: [7, 7, 8, 8, 9], str: [1, 1, 2, 2, 2],
    vis: 6, dist: 3, price: [8, 12, 16, 22, 28], hatch: 60, rb: ["spd", "dist"], leg: ["str", 1], solid2: true,
    skills: ["Carries food home", "x2 food from seeds and nuts"] },
  adventurer: { name: "Adventurer", short: "ADVENT.", castes: ["scout", "loader"], size: 1.0,
    hp: [6, 8, 9, 10, 12], dmg: [1, 1, 1, 2, 2], as: 1.4, cd: 2, cc: 0, spd: [6, 7, 8, 9, 10], str: 1,
    vis: 8, dist: [4, 9, 15, 24, 31], price: [5, 8, 10, 14, 18], hatch: 32, rb: ["spd", "hp"], leg: ["dmg", 1], fights: true,
    skills: ["Explores and brings food", "Fights enemies"] },
  seeker: { name: "Seeker", short: "SEEKER", castes: ["scout", "loader"], size: 1.0,
    hp: [4, 5, 7, 8, 10], dmg: 1, as: 1.4, cd: 2, cc: 0, spd: [6, 7, 8, 9, 10], str: [1, 1, 2, 2, 3],
    vis: 8, dist: [4, 9, 15, 24, 31], price: [5, 8, 12, 15, 19], hatch: 32, rb: ["spd", "dist"], leg: ["vis", 0.25], flees: true,
    skills: ["Explores and brings food", "Runs from enemies"] },
  carto: { name: "Cartographer", short: "CARTO", castes: ["scout"], size: 0.9,
    hp: [3, 4, 5, 6, 8], dmg: 1, as: 1.4, cd: 2, cc: 0, spd: [7, 8, 10, 11, 12], str: 0,
    vis: 8, dist: [5, 10, 17, 27, 37], price: [7, 10, 17, 27, 37], hatch: 32, rb: ["spd", "dist"], leg: ["vis", 0.25], flees: true,
    skills: ["Fastest: clears the fog", "Tells loaders about food", "Runs from enemies"] },
  attacker: { name: "Attacker", short: "ATTACKER", castes: ["warrior"], size: 1.4, barracks: 1,
    hp: [18, 22, 32, 38, 45], dmg: [1, 2, 2, 3, 4], as: 1, cd: 8, cc: [.05, .1, .15, .2, .25], spd: 7, str: 0,
    vis: [7, 8, 9, 10, 11], dist: 4, price: [16, 25, 30, 40, 50], hatch: 64, rb: ["hp", "vis"], leg: ["dmg", 1], fights: true,
    skills: ["Raids enemy bases", "Lives in barracks"] },
  dagger: { name: "Dagger", short: "DAGGER", castes: ["warrior"], size: 1.1, barracks: 0.5,
    hp: [10, 13, 17, 21, 26], dmg: [1, 2, 2, 3, 3], as: 0.7, cd: 4, cc: .1, spd: 9, str: 0,
    vis: [7, 8, 9, 10, 11], dist: 4, price: [10, 15, 20, 26, 32], hatch: 48, rb: ["hp", "spd"], leg: ["dmg", 1], fights: true,
    skills: ["Raids enemy bases", "Two per barracks", "Fast hits"] },
  giant: { name: "Giant", short: "GIANT", castes: ["warrior"], size: 1.9, bigBarracks: 1,
    hp: [60, 75, 95, 115, 140], dmg: [3, 4, 5, 6, 8], as: 1.6, cd: 10, cc: .1, spd: 4, str: 0,
    vis: [7, 8, 9, 10, 11], dist: 4, price: [40, 55, 70, 85, 100], hatch: 110, rb: ["hp", "dmg"], leg: ["spd", 1], fights: true, splash: true,
    skills: ["Raids enemy bases", "Lives in big barracks", "Hits all around"] },
  dart: { name: "Dart", short: "DART", castes: ["shooter", "warrior"], size: 1.1, barracks: 1,
    hp: [8, 10, 12, 14, 17], dmg: [1, 2, 2, 3, 3], as: 1.3, cd: 4, cc: .1, spd: 6, str: 0, range: 28,
    vis: [8, 9, 10, 11, 12], dist: 4, price: [14, 20, 26, 32, 40], hatch: 60, rb: ["dmg", "vis"], leg: ["dmg", 1], fights: true,
    skills: ["Raids enemy bases", "Shoots from range", "Lives in barracks"] },
  defender: { name: "Defender", short: "DEFENDER", castes: ["guardian"], size: 1.6,
    hp: [30, 45, 60, 80, 100], dmg: [1, 2, 3, 4, 5], as: 1, cd: 0, cc: .25, spd: 5, str: 0, stun: 1,
    vis: [7, 8, 9, 10, 11], dist: 2, price: [12, 14, 20, 25, 30], hatch: 96, rb: ["hp", "vis"], leg: ["dmg", 1], fights: true,
    maxSpace: [10, 20, 30, 40, 50],
    skills: ["Guards the nest entrance", "Stops thieves first", "Crit stuns 1 s"] },
  bodyguard: { name: "Bodyguard", short: "BODYGRD", castes: ["guardian"], size: 1.5,
    hp: [35, 50, 60, 70, 80], dmg: [1, 2, 3, 4, 5], as: 1, cd: 8, cc: 0, spd: 5, str: 0,
    vis: [7, 8, 9, 10, 11], dist: 2, price: [12, 16, 20, 25, 30], hatch: 96, rb: ["hp", "vis"], leg: ["dmg", 1], fights: true,
    maxSpace: [10, 20, 30, 40, 50],
    skills: ["Walks the food trails", "Protects loaders far away"] },
};
const QUEEN_RB = [{ hs: 1, slots: 0, qs: 0, workers: 0 }, { hs: 1.1, slots: 1, qs: 0, workers: 0 },
  { hs: 1.2, slots: 2, qs: -0.2, workers: 0 }, { hs: 1.3, slots: 3, qs: -0.3, workers: 2 }];

// Enemies. bdmg = damage to food piles. thief = steals food. wander = walks the map alone.
const MOBS = {
  ladybug: { name: "Ladybug", hp: 14, dmg: 1, as: 1.2, spd: 4.5, vis: 9, food: 8, chitin: .3, size: 1.4, col: "#d8382c", look: "bug" },
  lthief: { name: "Ladybug Thief", hp: 10, dmg: 1, as: 1.5, spd: 5, vis: 10, food: 8, chitin: .3, size: 1.2, col: "#e8783c", look: "bug", thief: true },
  beewar: { name: "Bee Warrior", hp: 20, dmg: 3, as: 1.2, spd: 6, vis: 10, food: 10, chitin: .4, size: 1.4, col: "#f2c230", look: "bee" },
  beescout: { name: "Bee Scout", hp: 8, dmg: 1, as: 1, spd: 7.5, vis: 12, food: 6, chitin: .3, size: 1.1, col: "#f6dc6a", look: "bee", thief: true },
  beequeen: { name: "Bee Queen", hp: 90, dmg: 4, as: 1.5, spd: 3.5, vis: 10, food: 40, chitin: 1, size: 2.2, col: "#ffb52e", look: "bee", boss: true },
  darkling: { name: "Darkling Beetle", hp: 90, dmg: 3, as: 2, spd: 3, vis: 8, food: 40, chitin: 1, size: 2.3, col: "#2c2a3a", look: "beetle", wander: true },
  termite: { name: "Termite Brown", hp: 16, dmg: 1, as: 1.2, spd: 5.2, vis: 10, food: 8, chitin: .2, size: 1.2, col: "#b07a48", look: "ant" },
  termwhite: { name: "Termite White", hp: 10, dmg: 1, as: 1.2, spd: 6, vis: 10, food: 6, chitin: .2, size: 1.1, col: "#efe2c8", look: "ant", thief: true },
  termqueen: { name: "Termite Queen", hp: 130, dmg: 2, as: 1.5, spd: 2, vis: 8, food: 50, chitin: 1, size: 2.6, col: "#e8cfa2", look: "grub", boss: true },
  scorpion: { name: "Scorpion Brown", hp: 35, dmg: 5, as: 2, spd: 4.5, vis: 10, food: 24, chitin: .75, size: 2.0, col: "#8a5a2e", look: "scorp", cc: .25, cdm: 5 },
  scorpblack: { name: "Scorpion Black", hp: 50, dmg: 6, as: 2.2, spd: 4.2, vis: 10, food: 30, chitin: .8, size: 2.1, col: "#2b2422", look: "scorp", cc: .25, cdm: 6 },
  scorpboss: { name: "Scorpion Boss", hp: 160, dmg: 9, as: 2.5, spd: 3.5, vis: 10, food: 60, chitin: 1, size: 2.8, col: "#5a1f12", look: "scorp", boss: true, cc: .25, cdm: 8 },
  camel: { name: "Camel Spider", hp: 36, dmg: 3, as: 1.2, spd: 7, vis: 12, food: 24, chitin: .6, size: 1.9, col: "#c79a5c", look: "spider" },
  scarab: { name: "Scarab", hp: 110, dmg: 3, as: 1.8, spd: 3, vis: 8, food: 50, chitin: 1, size: 2.4, col: "#2f7a6a", look: "beetle", wander: true },
};
// Enemy bases. tier 0/1/2 = simple/medium/difficult (sugar reward).
const BASES = {
  ladynest: { name: "Ladybug Nest", hp: 60, tier: 0, guard: ["ladybug", "ladybug", "ladybug"], raid: "ladybug", thief: "lthief", col: "#7b3a2a", look: "mound" },
  hive: { name: "Bee Hive", hp: 110, tier: 1, guard: ["beewar", "beewar", "beewar", "beequeen"], raid: "beewar", thief: "beescout", col: "#c8952c", look: "hive" },
  termound: { name: "Termite Mound", hp: 130, tier: 1, guard: ["termite", "termite", "termite", "termite", "termqueen"], raid: "termite", thief: "termwhite", col: "#a8743e", look: "tower" },
  scorpden: { name: "Scorpion Den", hp: 170, tier: 2, guard: ["scorpion", "scorpblack", "scorpboss"], raid: "scorpion", thief: null, col: "#5c3a26", look: "rock" },
  camelhole: { name: "Camel Spider Hole", hp: 100, tier: 1, guard: ["camel", "camel"], raid: "camel", thief: null, col: "#7a5a3a", look: "hole" },
};
const SUGAR_TIER = [[3, 7], [11, 18], [19, 26]];

// Underground buildings. size 1 = 1 tile, 2 = 2x2.
const BUILDS = {
  qs: { name: "Queen Room", short: "QUEEN", size: 1, cost: 12, col: "#c9a14a", desc: "Queen lives here. Keeps eggs (and food). 2 tiles = 1 queue slot" },
  fs: { name: "Food Stock", short: "FOOD", size: 1, cost: 9, col: "#7cae4a", desc: "Keeps 8 food" },
  cs: { name: "Chitin Stock", short: "CHITIN", size: 1, cost: 8, col: "#7aa6c4", desc: "Keeps 4 chitin. Opens the workshop" },
  br: { name: "Barracks", short: "BARRACK", size: 1, cost: 10, col: "#b85440", desc: "Home for 1 attacker (2 daggers, 1 dart)" },
  bb: { name: "Big Barracks", short: "BIG BAR", size: 2, cost: 30, col: "#8e3a30", desc: "Home for 1 giant" },
  ws: { name: "Workshop", short: "WORKSHP", size: 2, cost: 40, col: "#6e7f8c", desc: "Makes chitin plates to level up ants", needs: "cs" },
};

// Start boosts: choose 2 of 3, one refresh.
const BOOSTS = [
  { id: "food10", name: "+10 FOOD IN ALL FOOD SOURCES", icon: "food" },
  { id: "queen3", name: "QUEEN X3 HEALTH +2 DAMAGE", icon: "queen" },
  { id: "bar4", name: "+4 ATTACKER BARRACKS", icon: "barracks" },
  { id: "build2", name: "+2 BUILDERS AT START", icon: "builder" },
  { id: "store15", name: "+15 FOOD IN THE NEST", icon: "food" },
  { id: "hatch30", name: "EGGS HATCH 30 % FASTER", icon: "egg" },
  { id: "qroom2", name: "+2 QUEEN ROOMS (+1 QUEUE)", icon: "queen" },
  { id: "loadstr", name: "LOADERS CARRY +1", icon: "loader" },
  { id: "scout3", name: "+3 SEEKERS AT START", icon: "scout" },
  { id: "speed20", name: "ALL ANTS +20 % SPEED", icon: "speed" },
  { id: "chitin8", name: "+8 CHITIN + CHITIN STOCK", icon: "chitin" },
  { id: "guard1", name: "ENEMY BASES -1 GUARD", icon: "sword" },
];

// Perks: up to 3 active. need = unlock rule (stat >= n) or qp = unlocked by the queen path.
const PERKS = [
  { id: "carry", name: "Carrying Capacity", desc: "Builders 60 % slower but carry +1", need: ["wins", 3], needTxt: "Win 3 games" },
  { id: "carapace", name: "Thick Carapace", desc: "Queen x5 health, she can't fight", need: ["queenHits", 3], needTxt: "Survive 3 attacks on the queen" },
  { id: "kinder", name: "Kindergarten", desc: "Ants -20 % price, eggs hatch 5x slower", qp: true },
  { id: "golden", name: "Golden Eggs", desc: "Ants +25 % price, eggs hatch 4x faster", qp: true },
  { id: "bigger", name: "Bigger Pieces", desc: "Loaders carry +1, 1.7x slower", qp: true },
  { id: "heavy", name: "Heavy Bite", desc: "Loaders: 50 % chance +1 food, bites take longer", need: ["bestFood", 800], needTxt: "Collect 800 food in one game" },
  { id: "blitz", name: "Blitzkrieg", desc: "Raid size -5, raid ants +200 % speed", need: ["bestRaid", 12], needTxt: "Send 12 ants to one raid" },
  { id: "platoon", name: "Platoon", desc: "Raid size +5, raid ants -30 % speed", qp: true },
  { id: "lucky", name: "Lucky Collector", desc: "Scouts: 50 % chance of double food", need: ["scouts", 60], needTxt: "Make 60 scouts in total" },
  { id: "attent", name: "Attentiveness", desc: "Scouts see 1.5x, go 25 % less far", qp: true },
  { id: "newborn", name: "New Born", desc: "New loaders: +40 % speed, +1 carry for 120 s", need: ["bestEggs", 10], needTxt: "Have 10 eggs at once" },
  { id: "decisive", name: "Decisive Blow", desc: "All ants 20 % crit chance", need: ["kills", 200], needTxt: "Kill 200 enemies in total" },
  { id: "predator", name: "Predators", desc: "Scouts and loaders +1 damage", need: ["workerKills", 150], needTxt: "Kill 150 with scouts and loaders" },
  { id: "siege", name: "Siege", desc: "x2 damage to enemy bases", qp: true },
  { id: "diggers", name: "Builders", desc: "Builders dig and fill 2x faster", need: ["bestDig", 50], needTxt: "Dig 50 tiles in one game" },
  { id: "mother", name: "Motherly Care", desc: "Eggs next to the queen hatch 1.5x faster", need: ["bestEggsMade", 60], needTxt: "Lay 60 eggs in one game" },
  { id: "newbie", name: "Newbie", desc: "New loaders bite +1 food for 5 bites", need: ["loaders", 8], needTxt: "Make 8 loaders" },
];

// Levels. bases = base kinds placed around the nest; wander = lone big mobs.
const LOCS = [
  { id: "forest", name: "FOREST", ground: ["#4d7a35", "#5a8a3c", "#466f30", "#62913f"], soil: ["#6b4a2e", "#5a3d26", "#4a3220"],
    food: ["mushroom", "berry", "seed"], deco: "grass",
    levels: [
      { bases: ["ladynest", "ladynest"], wander: [], mul: 1.0, q: ["build_ws", "ants30"] },
      { bases: ["ladynest", "hive", "ladynest"], wander: ["darkling"], mul: 1.15, q: ["time25", "sugar10"] },
      { bases: ["ladynest", "hive", "ladynest", "hive"], wander: ["darkling"], mul: 1.25, q: ["lvl3", "noqueen"] },
    ] },
  { id: "semidesert", name: "SEMIDESERT", ground: ["#c9a85c", "#d4b468", "#bf9c52", "#d8bc78"], soil: ["#8a6a42", "#76583a", "#5e4630"],
    food: ["cactus", "seed", "mushroom"], deco: "desert",
    levels: [
      { bases: ["termound", "camelhole", "termound"], wander: ["scarab"], mul: 1.05, q: ["build_ws", "ants40"] },
      { bases: ["termound", "scorpden", "camelhole"], wander: ["scarab"], mul: 1.2, q: ["queen2", "time30"] },
      { bases: ["termound", "scorpden", "camelhole", "termound"], wander: ["scarab", "scarab"], mul: 1.35, q: ["queen2", "ants50"] },
    ] },
];
const QUESTS = {
  win: { txt: "Destroy every base" },
  build_ws: { txt: "Build a workshop" },
  ants30: { txt: "Grow the colony to 30 ants" },
  ants40: { txt: "Grow the colony to 40 ants" },
  ants50: { txt: "Grow the colony to 50 ants" },
  time25: { txt: "Win in under 25 minutes" },
  time30: { txt: "Win in under 30 minutes" },
  sugar10: { txt: "Find 10 sugar on the map" },
  lvl3: { txt: "Level an ant up to 3" },
  noqueen: { txt: "Queen never gets hit" },
  queen2: { txt: "Build a 2nd queen room group and move the queen" },
};
// Queen path: total stars -> reward
const QPATH = [
  { stars: 2, txt: "+30 sugar", give: { sugar: 30 } },
  { stars: 4, txt: "Rare Loader", give: { card: ["loader", 1] } },
  { stars: 6, txt: "Perk: Siege", give: { perk: "siege" } },
  { stars: 8, txt: "Hopper + Dagger cards", give: { cards: [["hopper", 0], ["dagger", 0]] } },
  { stars: 10, txt: "Perk: Platoon", give: { perk: "platoon" } },
  { stars: 12, txt: "Cartographer + Dart", give: { cards: [["carto", 0], ["dart", 0]] } },
  { stars: 14, txt: "Perk: Golden Eggs + Kindergarten", give: { perks: ["golden", "kinder"] } },
  { stars: 16, txt: "Giant + Skirmish Queen", give: { cards: [["giant", 0], ["squeen", 0]] } },
  { stars: 18, txt: "Perk: Bigger Pieces + Attentiveness", give: { perks: ["bigger", "attent"] } },
  { stars: 20, txt: "Queen of Fields + 100 sugar", give: { cards: [["fqueen", 0]], sugar: 100 } },
  { stars: 22, txt: "Epic Universal Queen", give: { card: ["uqueen", 2] } },
];
const FOODS = {
  mushroom: { name: "Mushroom", col: "#d8443a", col2: "#f3e2c8", amt: [48, 80] },
  berry: { name: "Berries", col: "#7a4bd0", col2: "#b896f0", amt: [32, 60] },
  seed: { name: "Seeds", col: "#e0b84a", col2: "#a4772c", amt: [40, 72], solid: true },
  cactus: { name: "Cactus Fruit", col: "#e8608a", col2: "#5aa04a", amt: [44, 76] },
  drop: { name: "Food Drop", col: "#e88a4a", col2: "#ffd2a0", amt: [0, 0] },
  cave: { name: "Cave Mushroom", col: "#c9b8ff", col2: "#7a64c8", amt: [30, 50] },
};
