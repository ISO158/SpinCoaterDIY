// ==========================================
// PROJETO SPIN COATER - VERSÃO FINAL (5mm ESTRUTURAL)
// ==========================================

// --- DIMENSÕES DO GABINETE (mm) ---
// Decisão Final: 5mm permite infill Gyroid funcional (efeito sanduíche)
// reduzindo a tensão térmica interna comparado a uma parede fina sólida.
espessura_base_topo = 5; 
espessura_laterais = 5;  

largura_total = 180;  
profundidade_total = 210; 
altura_total = 107;   

raio_arredondamento = 3; 

// --- GEOMETRIA DO TRAPÉZIO FRONTAL ---
altura_base_painel = 70;  
altura_topo_painel = 100; 
recuo_painel = 30; 

// --- POSIÇÃO DO BURACO DA FORMA ---
raio_forma = 80; // 16cm diâmetro
// Distância do fundo ajustada para parede de 5mm
y_centro_forma = profundidade_total - espessura_laterais - 5 - raio_forma;

// --- CÁLCULO DO ÂNGULO DO PAINEL ---
cateto_oposto = recuo_painel;
cateto_adjacente = altura_topo_painel - altura_base_painel;
angulo_painel = atan(cateto_oposto / cateto_adjacente); 

$fn = 100; 

module CorpoGabinete() {
    hull() {
        // --- PARTE TRASEIRA ---
        translate([raio_arredondamento, profundidade_total-raio_arredondamento, raio_arredondamento]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, profundidade_total-raio_arredondamento, raio_arredondamento]) sphere(r=raio_arredondamento);
        translate([raio_arredondamento, profundidade_total-raio_arredondamento, altura_total-raio_arredondamento]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, profundidade_total-raio_arredondamento, altura_total-raio_arredondamento]) sphere(r=raio_arredondamento);
        
        // --- PARTE FRONTAL ---
        translate([raio_arredondamento, raio_arredondamento, raio_arredondamento]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, raio_arredondamento, raio_arredondamento]) sphere(r=raio_arredondamento);
        
        translate([raio_arredondamento, raio_arredondamento, altura_base_painel]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, raio_arredondamento, altura_base_painel]) sphere(r=raio_arredondamento);
        
        translate([raio_arredondamento, recuo_painel + raio_arredondamento, altura_topo_painel]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, recuo_painel + raio_arredondamento, altura_topo_painel]) sphere(r=raio_arredondamento);
        
        translate([raio_arredondamento, recuo_painel + raio_arredondamento, altura_total-raio_arredondamento]) sphere(r=raio_arredondamento);
        translate([largura_total-raio_arredondamento, recuo_painel + raio_arredondamento, altura_total-raio_arredondamento]) sphere(r=raio_arredondamento);
    }
}

module GabineteFinal() {
    difference() {
        // 1. Sólido Externo
        CorpoGabinete();
        
        // 2. Oco Interno (AGORA UNIFORME 8mm)
        altura_interna = altura_total - (2 * espessura_base_topo);

        difference() {
            hull() {
                // Fundo Traseiro
                translate([espessura_laterais, profundidade_total-espessura_laterais, espessura_base_topo]) 
                cylinder(r=1, h=altura_interna);
                translate([largura_total-espessura_laterais, profundidade_total-espessura_laterais, espessura_base_topo]) 
                cylinder(r=1, h=altura_interna);
                
                // Frente Baixo
                translate([espessura_laterais, espessura_laterais, espessura_base_topo]) 
                cylinder(r=1, h=altura_base_painel - espessura_base_topo);
                translate([largura_total-espessura_laterais, espessura_laterais, espessura_base_topo]) 
                cylinder(r=1, h=altura_base_painel - espessura_base_topo);
                
                // Frente Angulo
                translate([espessura_laterais, recuo_painel+3, altura_topo_painel - espessura_base_topo]) cylinder(r=1, h=1);
                translate([largura_total-espessura_laterais, recuo_painel+3, altura_topo_painel - espessura_base_topo]) cylinder(r=1, h=1);
                    
                // Frente Topo
                translate([espessura_laterais, recuo_painel+3, altura_total - espessura_base_topo]) cylinder(r=1, h=1);
                translate([largura_total-espessura_laterais, recuo_painel+3, altura_total - espessura_base_topo]) cylinder(r=1, h=1);
            }
             translate([0,0,-5]) cube([largura_total, profundidade_total, 1]); 
        }
        
        // --- FURO DA FORMA ---
        translate([largura_total/2, y_centro_forma, altura_total - 20]) cylinder(h = 30, r = raio_forma+4);
        // Fresta de 2mm no topo (rebaixo visual)
        translate([largura_total/2, y_centro_forma, altura_total-2]) cylinder(r=raio_forma+4, h=3);

        // --- COMPONENTES FRONTAIS ---
        z_centro_face = (altura_base_painel + altura_topo_painel) / 2;
        y_centro_face = recuo_painel / 2; 
        
        translate([0, y_centro_face + 5, z_centro_face]) 
        rotate([angulo_painel, 0, 0]) 
        {
            // Display
            translate([2*largura_total/5 -(25), 0, 0]) cube([26, 17, 50], center=true);
            // Rebaixo visual do display (mantido do original)
            rotate([3.5, 0, 0]) translate([2*largura_total/5 -(25), 0, 0]) cube([15, 26, 8], center=true);
            
            // Encoder
            translate([largura_total/2 -(7.2), 0, 0]) cylinder(h=50, r=3.6, center=true);
            // Rebaixo visual do Encoder (mantido do original)
            rotate([3.5, 0, 0]) translate([largura_total/2 -(7.2), 0, 0]) cube([15, 15, 8], center=true); 
            
            // LEDs
            translate([3*largura_total/5 -(0), +6,0]) cylinder(h=50, r=2.6, center=true); 
            translate([3*largura_total/5 -(0),-6,0]) cylinder(h=50, r=2.6, center=true); 
            
            // Interruptor
            translate([4*largura_total/5 -(6.5), 0,0]) cube([13, 19, 50], center=true);
            // Rebaixo visual do Interruptor (mantido do original)
            rotate([3.5, 0, 0]) translate([4*largura_total/5 -(6.5), 0, 0]) cube([16, 22, 8], center=true);
        }
        
        // --- SAÍDA CABO ---
        translate([largura_total-largura_total/4, profundidade_total+10, 15]) 
        rotate([90,0,0])
        hull() { 
            cylinder(h=20, r=4);
            translate([0, 0, 0]) cylinder(h=20, r=4);
        }
        
        // --- VENTILAÇÃO ---
        for(lado = [0, largura_total]) {
            translate([lado, 2*profundidade_total/3, 40]) 
            rotate([0, 90, 0])
            for(i=[0:4]) {
                translate([0, i*12, -10])
                hull() {
                    cylinder(r=2, h=20);
                    translate([20, 0, 0]) cylinder(r=2, h=20);
                }
            }
        }
        
        // --- PÉS M8 ---
        posicoes_pes = [
            [20, 20],
            [largura_total-20, 20],
            [20, profundidade_total-20],
            [largura_total-20, profundidade_total-20]
        ];
    
        for(pos = posicoes_pes) {
            translate([pos[0], pos[1], -10]) 
            cylinder(r=4, h=30); 
        }
        
        // --- FUROS FORMA M6 NA BASE ---
        distancia_furos_forma = 100; 
        diametro_furo_m6 = 6.2; 
        translate([largura_total/2, y_centro_forma, 0]) {
            translate([distancia_furos_forma / 2, 0, 0]) cylinder(h=20, d=diametro_furo_m6, center=true);
            translate([distancia_furos_forma / 2, 0, 0]) cylinder(h=5, d=diametro_furo_m6+5, center=true);
            
            translate([-distancia_furos_forma / 2, 0, 0]) cylinder(h=20, d=diametro_furo_m6, center=true);
            translate([-distancia_furos_forma / 2, 0, 0]) cylinder(h=5, d=diametro_furo_m6+5, center=true);
            
            translate([0, distancia_furos_forma / 2, 0]) cylinder(h=20, d=diametro_furo_m6, center=true);
            translate([0, distancia_furos_forma / 2, 0]) cylinder(h=5, d=diametro_furo_m6+5, center=true);
            
            translate([0, -distancia_furos_forma / 2, 0]) cylinder(h=20, d=diametro_furo_m6, center=true);
            translate([0, -distancia_furos_forma / 2, 0]) cylinder(h=5, d=diametro_furo_m6+5, center=true);
        }
        
    }
}

// ==========================================
// --- MÓDULOS DE VISUALIZAÇÃO ---
// ==========================================

module FormaAluminio() {
    color("Silver", 0.8) {
        difference() {
            cylinder(r=raio_forma, h=97); 
            translate([0,0,2]) cylinder(r=raio_forma-2, h=95); 
        }
        translate([0,0,97]) difference() {
            cylinder(r=raio_forma+5, h=3);
        }
    }
}

module EVA_Amortecimento() {
    color("Black", 0.2) {
        cylinder(r=raio_forma-1, h=2); 
    }
}

module FonteChaveada() {
    color("Gold") cube([85, 30, 60]); 
}

module ESP32() {
    color("DarkGreen") cube([50, 10, 70]); 
}

module ESC_Brushless() {
    color("DodgerBlue") rotate([0,0,90]) {
        cube([50, 10, 25]); 
        color("Red") translate([50, 2, 5]) rotate([0,90,0]) cylinder(h=20, r=1.5);
        color("Black") translate([50, 5, 5]) rotate([0,90,0]) cylinder(h=20, r=1.5);
        color("Yellow") translate([50, 8, 5]) rotate([0,90,0]) cylinder(h=20, r=1.5);
    }
}

module MotorBLDC() {
    color("DimGray") {
        cylinder(r=14, h=30); 
        translate([0,0,-10]) cylinder(r=3, h=50); 
        translate([0,0,-2]) cube([35, 35, 2], center=true);
    }
}

module VisualizacaoMontagem() {
    %GabineteFinal(); 
    
    translate([largura_total/2, y_centro_forma, 5]) EVA_Amortecimento();
    translate([largura_total/2, y_centro_forma, 7]) FormaAluminio();
    translate([largura_total/2, y_centro_forma, 7]) MotorBLDC();
    
    translate([largura_total/2 - 45, 5, 5]) FonteChaveada();
    translate([15, 20, 5]) rotate([0, 0, 90]) ESP32();
    translate([largura_total-5, 5, 5]) ESC_Brushless();
}

GabineteFinal();