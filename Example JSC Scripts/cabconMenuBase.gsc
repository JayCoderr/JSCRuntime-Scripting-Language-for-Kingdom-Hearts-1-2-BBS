init()
{
    sora onPlayerConnect();
}

onPlayerConnect()
{	
	wait 0.5;
	sora onPlayerSpawned();
}

onPlayerSpawned()
{
    sora iPrintln("^1cabconmodding.com");
	sora freezeControls(false);
    if(!isDefined(sora.menu["active"]))
    {	
		sora = spawnStruct();
        sora init_menuSystem();
		sora.menu["active"] = true;
        sora iprintln($"Welcome to {sora.menu["name"]} ^7for KH1");
        sora iprintln(&"By ^2CabCon {urlimage(https://iili.io/nmguPwP.png, 30, 30)}");
        sora initMenuOpts();
        sora thread initMenu();
		sora thread textbarShit();
    }
}

init_menuSystem()
{
    sora.menu["name"] = &"{urlimage(https://iili.io/nmguPwP.png, 30, 30)} ^7KH Menu Base {urlimage(https://iili.io/nmguPwP.png, 30, 30)}"; 
}

initMenuOpts()
{
	//main menu
    sora addMenu("main", sora.menu["name"], undefined);
    sora addOpt("main", "Main Menu", &subMenu, "main_mods");
    sora addOpt("main", "Weapons", &subMenu, "main_mods");
    sora addOpt("main", "Game Settings", &subMenu, "main_mods");
    sora addOpt("main", "Models", &subMenu, "main_mods");
    sora addOpt("main", "Bullets", &subMenu, "main_mods");
    sora addOpt("main", "Messages", &subMenu, "main_mods");
    sora addOpt("main", "Perks", &subMenu, "main_mods");
    sora addOpt("main", "Admin Menu", &subMenu, "main_mods");
    sora addOpt("main", "Host Menu", &subMenu, "main_mods");
    sora addOpt("main", "Aimbot", &subMenu, "main_mods");
    sora addOpt("main", "Clients", &subMenu, "main_mods");
    sora addOpt("main", "All Clients", &subMenu, "main_mods");
    
	//main mods
    sora addMenu("main_mods", "Main Mods", "main");
	sora addOpt("main_mods", "God Mode", &test, undefined);
    sora addOpt("main_mods", "Unlimited Ammo", &test, undefined);
    sora addOpt("main_mods", "Fly Mode", &FlyMode, undefined);//
    sora addOpt("main_mods", "Field of View", &test, undefined);
    sora addOpt("main_mods", "Print something!", &test, undefined);
}
 
initMenu()
{
	sora.openBox = sora createRectangle("CENTER", "CENTER", 1050, 0, 465, 1150, (0,0,0), "white", 1, 0);
	
    sora.currentMenu = "main";
    sora.menuCurs = 0;
    for(;;)
    {
		if(sora ButtonPressed("L2") && sora ButtonPressed("R3"))
		{
			if(!isDefined(sora.inMenu))
			{
				sora.inMenu	= true;
				sora.openText = sora createText("default", 3, "TOP", "RIGHT",  1175, 20, 2, 1, (1,1,1), sora.menu["name"]);
				sora.openBox.alpha = .7;				
				menuOpts = sora.menuAction[sora.currentMenu].opt.size;
				//sora.openBox scaleOverTime(.4, 200, ((455)+45));//technically not a real function but can add it by default... not even technically needed since u can just make it...	
				wait.5;
				sora.openText = sora settext(sora.menuAction[sora.currentMenu].title);////
				string = "";
				for(m = 0; m < menuOpts; m++)
				{
				    string+= sora.menuAction[sora.currentMenu].opt[m]+"\n";
				}
				sora.menuText = sora createText("default", 2.5, "TOP", "TOP", 1175, 100, 3, 1, (1,1,1), string);
				sora.scrollBar = sora createRectangle("CENTER", "CENTER", sora.openBox.x, 100, 465, 25, (0,40,1), "white", 2, 1);
				//sora thread textbarShit();
			}
		}
	    if(isDefined(sora.inMenu))
		{		
			if(sora ButtonPressed("R2"))
			{
				sora.menuCurs++;//seems to work += 1
				if(sora.menuCurs > sora.menuAction[sora.currentMenu].opt.size-1)
					sora.menuCurs = 0;
				
				//self.scrollBar moveOverTime(.15);
							
				sora.scrollBar.y = ((sora.menuCurs*30)+((sora.menuText.y+2.5)-(30/7)));
				wait .15;
			}
			else if(sora ButtonPressed("L2"))
			{
				sora.menuCurs--;
				
				//self.scrollBar moveOverTime(.15);
				
				if(sora.menuCurs < 0)
					sora.menuCurs = sora.menuAction[sora.currentMenu].opt.size-1;
			
				sora.scrollBar.y = ((sora.menuCurs*30)+((sora.menuText.y+2.5)-(30/7)));
				wait .15;
			}
			else if(sora ButtonPressed("SQUARE") || sora ButtonPressed("F"))
			{		
				sora thread [[sora.menuAction[sora.currentMenu].func[sora.menuCurs]]](sora.menuAction[sora.currentMenu].inp[sora.menuCurs]);
				//wait.2;
			}
			else if(sora ButtonPressed("R3") || sora ButtonPressed("V"))
			{
				if(!isDefined(sora.menuAction[sora.currentMenu].parent))
				{
				    sora func_menuexiut();
				}
				else
				{
					sora subMenu(sora.menuAction[sora.currentMenu].parent);					
				}
			}
		}
		if(sora ButtonPressed("DPAD_LEFT") || sora ButtonPressed("F2"))
		{				
			sora FlyMode();
		}		
    }
    wait .05;
}

textbarShit()
{								                
    sora.Void.TestData["HUD"]["newsbarbg"] = sora createRectangle("BOTTOM", "LEFT", 0, 1000, 2000, 40, (0,0,0), "white", 1, 0.75);		
    sora.Void.TestData["HUD"]["newsbartop"] = sora createRectangle("BOTTOM", "LEFT", 0, 1000, 2000, 2, (0,40,1), "white", 1, 0.75);
    sora.Void.TestData["HUD"]["newsbarbot"] = sora createRectangle("BOTTOM", "LEFT", 0, 1040, 2000, 2, (0,40,1), "white", 1, 0.75);
    sora.Void.TestData["HUD"]["newsbarTxt"] = sora createText("default", 1.5, "BOTTOM", "LEFT", 0, 1010, 1, 1, (1,1,1), &"JSC TEST MENU {urlimage(https://iili.io/n1cYttI.webp, 30, 30)}");
	speed = 0.002;
	while(1)
	{
		for(x = -200; x < 1500; x++)
		{
			sora.Void.TestData["HUD"]["newsbarTxt"].x = x;
			wait speed;
		}
	
		sora.Void.TestData["HUD"]["newsbarTxt"].x = -200;
	}
}

func_menuexiut()
{
    sora.inMenu = undefined;
    sora.openText destroy();                   
    //sora.openBox scaleOverTime(.4, 200, 30);//not really needed but i will add it
    sora.menuText destroy();
    sora.scrollBar destroy();
    sora.openBox.alpha = 0;
    //wait.4;
    if(!sora.KingdomHeartVars["PeterPanFly"])
    {
        sora freezecontrols(false);
    }
}

subMenu(menu)
{
    sora.menuCurs = 0;
    sora.currentMenu = menu;
	//self.scrollBar moveOverTime(.2);
	sora.scrollBar.y = ((sora.menuCurs*30)+((sora.menuText.y+2.5)-(30/7)));
    sora.menuText destroy();
	sora.openText = sora setText(sora.menuAction[sora.currentMenu].title);
    menuOpts = sora.menuAction[sora.currentMenu].opt.size;
    //wait .2;
    string = "";
    for(m = 0;m <menuOpts;m++)
    {
        string+= sora.menuAction[sora.currentMenu].opt[m] + "\n";
    }
	sora.menuText = sora createText("default", 2.5, "TOP", "TOP", 1175, 100, 3, 1, (1,1,1), string);
    //wait .2;
}

FlyMode()
{
	if(!sora.KingdomHeartVars["PeterPanFly"])
	{
		sora SetEntityState("Sora", "PeterPanFly");
		sora.KingdomHeartVars["PeterPanFly"] = true;
	}
	else
	{
		sora SetEntityState("Sora", "Reset");			
		sora.KingdomHeartVars["PeterPanFly"] = false;
	}	
}
 
test()
{
    sora iPrintln("^9Comming Soon");
}

addMenu(menu, title, parent)
{
    sora.menuAction[menu].title = title;
    sora.menuAction[menu].parent = parent;
}
 
addOpt(menu, opt, func, inp)
{
    m = sora.menuAction[menu].opt.size;
    sora.menuAction[menu].opt[m] = opt;
    sora.menuAction[menu].func[m] = func;
    sora.menuAction[menu].inp[m] = inp;
}

createText(font, fontScale, align, relative, x, y, sort, alpha, color, text)
{
    elemTxt = sora newClientHudElem(sora);
    elemTxt.align = align;
    elemTxt.relative = relative;
    elemTxt.fontName = font;
    elemTxt.x = x;
    elemTxt.y = y;
    elemTxt.fontSize = fontScale;
    elemTxt.sort = sort;
    elemTxt.color = color;
    elemTxt.alpha = alpha;
    elemTxt.text = text;
    return elemTxt;
}

createRectangle(align, relative, x, y, width, height, color, shader, sort, alpha)
{
    elem = sora newClientHudElem(sora);
    elem.width = width;
    elem.height = height;
    elem.align = align;
    elem.relative = relative;
    elem.x = x;
    elem.y = y;
    elem.sort = sort;
    elem.color = color;
    elem.alpha = alpha;
    elem.shader = shader;
    return elem;
}
