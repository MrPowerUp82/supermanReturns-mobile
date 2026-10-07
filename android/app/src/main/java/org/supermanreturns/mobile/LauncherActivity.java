package org.supermanreturns.mobile;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Arrays;

public final class LauncherActivity extends Activity {
    private static final int PICK_ISO=7, GOLD=0xffe8b350, MUTED=0xffa8b9cc, WHITE=0xfff4f5f7;
    private InstallController installer;
    private TextView status, gpu;
    private Button select, cancel, diagnostic,play;
    private ProgressBar progress;
    private final InstallController.Listener listener=this::render;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state); installer=InstallController.get(this);
        ScrollView scroll=new ScrollView(this); scroll.setFillViewport(true);
        LinearLayout column=new LinearLayout(this); column.setOrientation(LinearLayout.VERTICAL);
        column.setPadding(dp(26),dp(28),dp(26),dp(28)); scroll.addView(column); setContentView(scroll);
        scroll.setOnApplyWindowInsetsListener((view,insets) -> {
            var bars=insets.getInsets(android.view.WindowInsets.Type.systemBars() | android.view.WindowInsets.Type.displayCutout());
            view.setPadding(bars.left,bars.top,bars.right,bars.bottom); return insets;
        });
        text(column,"PROJETO EXPERIMENTAL  /  ANDROID ARM64",11,GOLD,true);
        space(column,28);
        text(column,"SUPERMAN",38,WHITE,true); text(column,"RETURNS",38,GOLD,true);
        space(column,10); text(column,"MOBILE",14,MUTED,true);
        space(column,30);
        LinearLayout info=card(column);
        text(info,"Port experimental",20,WHITE,true);
        space(info,8);
        text(info,BuildConfig.HAS_GAME_RUNTIME ? "Importe os arquivos do seu jogo e inicie o runtime experimental com Vulkan e controles touch." : "Importe os arquivos do seu jogo e teste Vulkan e controles. O runtime do jogo não está incluído nesta compilação.",15,MUTED,false);
        space(column,18);
        LinearLayout device=card(column);
        text(device,Build.MANUFACTURER + " " + Build.MODEL,17,WHITE,true);
        text(device,"Android " + Build.VERSION.RELEASE + " · " + String.join(", ",Build.SUPPORTED_ABIS),13,MUTED,false);
        gpu=text(device,"Consultando GPU…",13,MUTED,false);
        space(device,12);
        text(device,"ALVO INICIAL: GALAXY S22 SNAPDRAGON",11,GOLD,true);
        text(device,"Perfil planejado: 30 FPS · Vulkan do sistema. O diagnóstico não mede o desempenho do jogo.",13,MUTED,false);
        space(column,22);
        text(column,"ARQUIVOS DO JOGO",12,GOLD,true); space(column,8);
        status=text(column,"",15,WHITE,false);
        progress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal); progress.setMax(100);
        column.addView(progress,new LinearLayout.LayoutParams(-1,dp(12))); space(column,8);
        select=button(column,"Selecionar minha ISO",true);
        select.setOnClickListener(v -> {
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(intent,PICK_ISO);
        });
        cancel=button(column,"Cancelar importação",false); cancel.setOnClickListener(v -> installer.cancel());
        play=button(column,"Iniciar jogo experimental",true);play.setEnabled(false);
        play.setOnClickListener(v -> startActivity(new Intent(this,GameActivity.class)));
        diagnostic=button(column,"Testar Vulkan e controles",false);
        diagnostic.setEnabled(false);
        diagnostic.setOnClickListener(v -> startActivity(new Intent(this,DiagnosticsActivity.class)));
        space(column,22);
        text(column,"Xbox 360 · Title ID 454107ED\nUse sua própria cópia. Nenhum arquivo do jogo é incluído no APK. A importação ocorre somente neste aparelho.",12,MUTED,false);
        space(column,18); text(column,BuildConfig.HAS_GAME_RUNTIME ? "0.1.0-dev · runtime experimental" : "0.1.0-dev · diagnóstico",11,GOLD,false);
        new Thread(() -> {
            String result;
            try { result=NativeBridge.probe(); } catch(UnsatisfiedLinkError | RuntimeException e) { result="Biblioteca nativa indisponível: " + e.getMessage(); }
            final String message=result;
            runOnUiThread(() -> { if(!isDestroyed()) { gpu.setText(message); diagnostic.setEnabled(Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a") && !message.startsWith("ERRO:")); } });
        },"vulkan-probe").start();
    }
    @Override protected void onStart() { super.onStart(); installer.attach(listener); }
    @Override protected void onStop() { installer.detach(listener); super.onStop(); }
    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request==PICK_ISO && result==RESULT_OK && data!=null && data.getData()!=null) installer.install(data.getData());
    }
    private void render(InstallController.State state) {
        status.setText((state.installed()?"✓ Instalação preservada\n":"") + state.message());
        select.setEnabled(!state.busy()); cancel.setVisibility(state.busy()?View.VISIBLE:View.GONE);
        play.setEnabled(BuildConfig.HAS_GAME_RUNTIME && state.installed() && !state.busy());
        progress.setVisibility(state.busy()?View.VISIBLE:View.GONE);
        progress.setIndeterminate(state.percent()<0); progress.setProgress(Math.max(0,state.percent()));
    }
    private int dp(int value) { return Math.round(value*getResources().getDisplayMetrics().density); }
    private void space(LinearLayout box,int height) { View v=new View(this); box.addView(v,new LinearLayout.LayoutParams(1,dp(height))); }
    private LinearLayout card(LinearLayout parent) {
        LinearLayout box=new LinearLayout(this); box.setOrientation(LinearLayout.VERTICAL); box.setPadding(dp(18),dp(18),dp(18),dp(18));
        GradientDrawable bg=new GradientDrawable(); bg.setColor(0xff11243a); bg.setCornerRadius(dp(16)); box.setBackground(bg);
        parent.addView(box,new LinearLayout.LayoutParams(-1,-2)); return box;
    }
    private TextView text(LinearLayout box,String value,int size,int color,boolean bold) {
        TextView v=new TextView(this); v.setText(value); v.setTextSize(size); v.setTextColor(color); v.setLineSpacing(dp(3),1);
        if(bold) v.setTypeface(Typeface.DEFAULT,Typeface.BOLD); box.addView(v,new LinearLayout.LayoutParams(-1,-2)); return v;
    }
    private Button button(LinearLayout box,String value,boolean primary) {
        Button b=new Button(this); b.setText(value); b.setAllCaps(false); b.setTextSize(15); b.setTextColor(primary?0xff081526:WHITE);
        GradientDrawable bg=new GradientDrawable(); bg.setColor(primary?GOLD:0xff19334e); bg.setCornerRadius(dp(12)); b.setBackground(bg);
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,dp(54)); params.topMargin=dp(10); box.addView(b,params); return b;
    }
}
