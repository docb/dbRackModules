#ifndef DBRACKMODULES_HEXUTIL_H
#define DBRACKMODULES_HEXUTIL_H

static std::string getRandomHex(RND &rnd,float dens,int randomLengthFrom,int randomLengthTo) {
  unsigned long val=0;
  int len=rnd.nextRange(randomLengthFrom,randomLengthTo);
  for(int k=0;k<len;k++) {
    std::stringstream rstream;
    for(int j=0;j<4;j++) {
      if(rnd.nextCoin(1-dens)) {
        rstream<<"1";
      } else {
        rstream<<"0";
      }
    }
    val|=std::bitset<4>(rstream.str()).to_ullong()<<4*k;
  }
  //unsigned long val = rnd.next();
  std::stringstream stream;
  stream<<std::uppercase<<std::setfill('0')<<std::setw(len)<<std::hex<<val;
  return stream.str();
}

struct IntSelectItem : MenuItem {
  int *value;
  int min;
  int max;

  IntSelectItem(int *val,int _min,int _max) : value(val),min(_min),max(_max) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(int c=min;c<=max;c++) {
      menu->addChild(createCheckMenuItem(string::f("%d",c),"",[=]() {
        return *value==c;
      },[=]() {
        *value=c;
      }));
    }
    return menu;
  }
};

#endif //DBRACKMODULES_HEXUTIL_H
