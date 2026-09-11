/* Stock LF2 network input adapter. The retail 22-byte packet carries only
   directions plus defend/jump/attack and the always-on baseline bit 0x01.
   Vita-only convenience actions must never leak into the stock simulation. */
static void strip_vita_only_shortcuts(input_t *in){
    if(!in)return;
    in->run=false;
    in->pickup=false;
    in->pickup_pressed=false;
    in->special_attack=false;
    in->special_jump=false;
    in->special_attack_pressed=false;
    in->special_jump_pressed=false;
}

static uint32_t basic_remote_mask_from_input(const input_t *in){
    uint32_t m=0x01u;
    if(!in)return m;
    if(in->defend)m|=0x02u;
    if(in->jump)m|=0x04u;
    if(in->attack)m|=0x08u;
    if(in->right)m|=0x10u;
    if(in->left)m|=0x20u;
    if(in->up)m|=0x40u;
    if(in->down)m|=0x80u;
    return m;
}

static input_t make_remote_input(uint32_t held,uint32_t pressed){
    input_t in={0};
    in.defend=(held&0x02u)!=0;
    in.jump=(held&0x04u)!=0;
    in.attack=(held&0x08u)!=0;
    in.right=(held&0x10u)!=0;
    in.left=(held&0x20u)!=0;
    in.up=(held&0x40u)!=0;
    in.down=(held&0x80u)!=0;
    in.defend_pressed=(pressed&0x02u)!=0;
    in.jump_pressed=(pressed&0x04u)!=0;
    in.attack_pressed=(pressed&0x08u)!=0;
    return in;
}
