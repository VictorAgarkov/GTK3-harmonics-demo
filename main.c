#include <stdlib.h>
#include <gtk/gtk.h>
//#include <cairo.h>
#include <math.h>

#include "goods.h"

//--------------------------------------------------------------------------------------
G_MODULE_EXPORT gboolean draw_area_on_draw (GtkWidget *widget, cairo_t *cr, gpointer data);
G_MODULE_EXPORT void on_MagPhase_changed (GtkAdjustment *adjustment, gpointer user_data);
G_MODULE_EXPORT void on_btn_Phase0_clicked (GtkButton *button, gpointer data);

G_MODULE_EXPORT void on_adj_Common_value_changed (GtkAdjustment *adjustment, gpointer user_data);
G_MODULE_EXPORT void on_HarmNum_changed (GtkAdjustment *adjustment, gpointer user_data);

G_MODULE_EXPORT void on_btn_CommPhase0_clicked (GtkButton *button, gpointer user_data);
G_MODULE_EXPORT void on_btn_Amplify0_clicked (GtkButton *button, gpointer user_data);
G_MODULE_EXPORT void on_btn_CommDC0_clicked (GtkButton *button, gpointer user_data);

G_MODULE_EXPORT void on_cbx_Preset_changed (GtkComboBox *combobox, gpointer user_data);

extern int32_t sine_approx_table_256_order_1_acs_5 [];
//--------------------------------------------------------------------------------------
typedef struct
{
	GtkWidget      *label;
	GtkWidget      *scale;
	GtkAdjustment  *adj;
	//double          val;     // сюда переписывается значение из adj
	int32_t         val;     // сюда переписывается значение из adj fix.point 0.32
}
s_HarmonicScale;

typedef struct
{
	GtkWidget      *separ;       // разделитель слева от бокса
	GtkWidget      *box;         // здесь будут все контролы
	GtkWidget      *label;       // надпись - номер гармоники
	s_HarmonicScale mag, phase;  // контролы для амплитуды и фазы
	GtkWidget      *btn_phase0;  // сброс фазы
	GtkWidget      *chk_enabled; // чекбокс - разрешение
	gboolean        en;
}
s_HarmonicWidgets;
//--------------------------------------------------------------------------------------
/* виджеты */
GtkWidget     *window_main;
GtkWidget     *HarmBox;
GtkWidget     *cbx_Presets;
GtkAdjustment *adj_HarmNum;
GtkAdjustment *adj_CommonPhase;
GtkAdjustment *adj_CommonDC;
GtkAdjustment *adj_Amplify;


s_HarmonicWidgets harm_widgets[55];

GtkWidget     *window_draw;
GtkWidget     *drawing_area;

gboolean harmonics_in_update = TRUE;  // устанавливаем в 1, когда массово обновляем параметры гармоник (что бы не рисовать много)

//-----------------------------------------------------------------------------------------------------------------
int32_t get_sine_int32(const int32_t* table, int32_t pN, int pw, int ac_shift, uint32_t angle)
{
	// вычисляем синус угла angle, используя таблицу table, состоящей из
	// 2^pN строк и полином степени pw
	// angle делится на целую часть (pN бит) и дробную часть - смещение внутри отрезка
	// Вычисляем значение степенным рядом типа:
	// ((a[0] * x + a[1]) * x + a[2]) * x + a[3]

	int idx = angle >> (32 - pN); // индекс строки в таблице (целая часть)
	uint32_t X = (angle << pN) >> ac_shift; // дробная часть угла (фикс. точка 0.32)
	const int32_t* A = table + idx * (pw + 1); // указатель на нужную строку в таблице

	int64_t sum = A[0];
	for(int i = 1; i <= pw; i++)
	{
		sum *= X;

		#ifdef RINT
			sum += 0x80000000L;  // add 0.5 before round
		#endif // RINT

		sum >>= 32;
		sum += A[i];
	}
	return sum;
}


//--------------------------------------------------------------------------------------
double limit_degree(double angle)
{
	//   -180..+180
	int turns = (angle + 180) / 360;  //  number of turns
	return angle - turns * 360;
}
//---------------------------------------------------------------------------------------
void create_mag_phase_scale(s_HarmonicScale *scale, int type, gpointer user_data)
{
	int digits = 0;
	if(type == 0) // magnitude
	{
		digits = 3;
		scale->label = gtk_label_new("Mag.:");
		scale->adj   = gtk_adjustment_new (0, 0, 1, 0.001, 0.02, 0);
	}
	else if(type == 1) // phase
	{
		digits = 1;
		scale->label = gtk_label_new("Phase:");
		scale->adj   = gtk_adjustment_new (0, -180, 180, 0.1, 10, 0);
	}
	scale->scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, scale->adj);
	gtk_scale_set_digits  (GTK_SCALE(scale->scale), digits);
	gtk_widget_set_size_request((scale->scale), -1, 200);
	gtk_range_set_inverted(GTK_RANGE(scale->scale), TRUE);

	g_signal_connect(scale->adj, "value_changed", G_CALLBACK(on_MagPhase_changed), user_data);


}
//---------------------------------------------------------------------------------------
void add_mag_phase_scale_2_box(GtkWidget *box, s_HarmonicScale *scale)
{
	gtk_box_pack_start(GTK_BOX(box), scale->label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), scale->scale, FALSE, FALSE, 0);
}
//--------------------------------------------------------------------------------------

void create_main_window (void)
{
	GtkWidget *main_box;
	GtkWidget *box, *w1,  *w2, *w3;

	// adjustments
	adj_Amplify     = gtk_adjustment_new(0, -60,    12, 0.1, 2, 0);
	adj_CommonPhase = gtk_adjustment_new(0, -180,   180, 0.1, 10, 0);
	adj_CommonDC    = gtk_adjustment_new(0, -1, 1,  0.01, 0.1, 0);
	adj_HarmNum     = gtk_adjustment_new(5, 0, NUMOFARRAY(harm_widgets), 1, 10, 0);

	g_signal_connect (adj_Amplify,     "value-changed", G_CALLBACK (on_adj_Common_value_changed), NULL);
	g_signal_connect (adj_CommonPhase, "value-changed", G_CALLBACK (on_adj_Common_value_changed), NULL);
	g_signal_connect (adj_CommonDC,    "value-changed", G_CALLBACK (on_adj_Common_value_changed), NULL);
	g_signal_connect (adj_HarmNum,     "value-changed", G_CALLBACK (on_HarmNum_changed),          NULL);

	// main window
	window_main     = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect (window_main, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	gtk_window_set_title (GTK_WINDOW(window_main), "Harmonics demo (GTK3)");

	main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_widget_set_margin_end    (main_box, 5);
	gtk_widget_set_margin_start  (main_box, 5);
	gtk_widget_set_margin_top    (main_box, 5);
	gtk_widget_set_margin_bottom (main_box, 5);
	gtk_container_add (GTK_CONTAINER (window_main), main_box);

	// "harmonics number": label + scale
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (main_box), box);

	w1 = gtk_label_new("Harmonics number:");
	gtk_container_add (GTK_CONTAINER (box), w1);

	w1 = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(adj_HarmNum));
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_widget_set_size_request(w1, 280, -1);
	gtk_widget_set_hexpand(w1, TRUE);
	gtk_range_set_restrict_to_fill_level (GTK_RANGE(w1), FALSE);
	gtk_range_set_round_digits (GTK_RANGE(w1), 0);
	gtk_scale_set_digits  (GTK_SCALE(w1), 0);
	gtk_scale_set_value_pos (GTK_SCALE(w1), GTK_POS_RIGHT);

	// "common phase": label + scale + button
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (main_box), box);

	w1 = gtk_label_new("Common phase, º");
	gtk_container_add (GTK_CONTAINER (box), w1);

	w1 = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(adj_CommonPhase));
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_widget_set_hexpand(w1, TRUE);
	//gtk_range_set_fill_level(GTK_RANGE(w1), 0);
	gtk_range_set_restrict_to_fill_level (GTK_RANGE(w1), FALSE);
	gtk_range_set_round_digits (GTK_RANGE(w1), 1);
	gtk_scale_set_digits  (GTK_SCALE(w1), 0);
	gtk_scale_set_value_pos (GTK_SCALE(w1), GTK_POS_RIGHT);

	w1 = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_button_set_label(GTK_BUTTON(w1), ">0<");
	g_signal_connect(w1, "clicked", G_CALLBACK(on_btn_CommPhase0_clicked), NULL);


	// "DC offset": label + scale + button
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (main_box), box);

	w1 = gtk_label_new("DC offset:");
	gtk_container_add (GTK_CONTAINER (box), w1);

	w1 = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(adj_CommonDC));
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_widget_set_hexpand(w1, TRUE);
	//gtk_range_set_fill_level(GTK_RANGE(w1), 0);
	gtk_range_set_restrict_to_fill_level (GTK_RANGE(w1), FALSE);
	gtk_range_set_round_digits (GTK_RANGE(w1), 1);
	gtk_scale_set_digits  (GTK_SCALE(w1), 2);
	gtk_scale_set_value_pos (GTK_SCALE(w1), GTK_POS_RIGHT);

	w1 = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_button_set_label(GTK_BUTTON(w1), ">0<");
	g_signal_connect(w1, "clicked", G_CALLBACK(on_btn_CommDC0_clicked), NULL);


	// "amplify": label + scale + button
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (main_box), box);

	w1 = gtk_label_new("Amplify, db");
	gtk_container_add (GTK_CONTAINER (box), w1);

	w1 = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(adj_Amplify));
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_widget_set_hexpand(w1, TRUE);
	//gtk_range_set_fill_level(GTK_RANGE(w1), 0);
	gtk_range_set_restrict_to_fill_level (GTK_RANGE(w1), FALSE);
	gtk_range_set_round_digits (GTK_RANGE(w1), 1);
	gtk_scale_set_digits  (GTK_SCALE(w1), 1);
	gtk_scale_set_value_pos (GTK_SCALE(w1), GTK_POS_RIGHT);

	w1 = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_button_set_label(GTK_BUTTON(w1), ">0<");
	g_signal_connect(w1, "clicked", G_CALLBACK(on_btn_Amplify0_clicked), NULL);

	// presets: label + combobox
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (main_box), box);

	w1 = gtk_label_new("Preset:");
	gtk_container_add (GTK_CONTAINER (box), w1);

	w1 = gtk_combo_box_new();
	gtk_container_add (GTK_CONTAINER (box), w1);
	gtk_widget_set_margin_start  (w1, 19);
	gtk_widget_set_hexpand(w1, TRUE);
	g_signal_connect(w1, "changed", G_CALLBACK(on_cbx_Preset_changed), NULL);

	// список пресетов
	char *presets[] =
	{
		"First harmonic",  "Meander",          "Meander (45º)", "Sawtooth", "Triangle",
		"Single pulse (peak)", "Dual peak",  "Dual peak (45º)",
	};

	GtkTreeIter iter;
	GtkListStore  *list = gtk_list_store_new(1, G_TYPE_STRING);
	for(int i = 0; i < NUMOFARRAY(presets); i++)
	{
		gtk_list_store_append (list, &iter);
		gtk_list_store_set (list, &iter, 0, presets[i], -1);
	}
	gtk_combo_box_set_model(GTK_COMBO_BOX(w1), GTK_TREE_MODEL(list));
	g_object_unref(list);
	GtkCellRenderer* cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(w1), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(w1), cell, "text", 0, NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w1), 0);

	// сепаратор перед набором гармоник
	w1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (main_box), w1);

	// "harmonics set label": label
	w1 = gtk_label_new("Harmonics set:");
	gtk_container_add (GTK_CONTAINER (main_box), w1);
	//gtk_widget_set_hexpand(w1, TRUE);

	// harmonics set: ScrolledWindow (w3)  <- ViewPort (w2) <- Box (HarmBox)
	w3 = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add (GTK_CONTAINER (main_box), w3);
	gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW(w3), FALSE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(w3), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type  (GTK_SCROLLED_WINDOW(w3), GTK_SHADOW_ETCHED_OUT);

	w2 = gtk_viewport_new(NULL, NULL);
	gtk_container_add (GTK_CONTAINER (w3), w2);

	HarmBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (w2), HarmBox);
	gtk_widget_set_margin_top    (HarmBox, 5);
	gtk_widget_set_margin_bottom (HarmBox, 5);


}

//---------------------------------------------------------------------------------------
void create_harmset(void)
{
	//grid_HarmSet
	int harm_total = NUMOFARRAY(harm_widgets);

	gtk_adjustment_set_upper(adj_HarmNum, harm_total);

	// создаём контролы для каждой гармоники и втыкаем их в grid_HarmSet
	for(int i = 0; i < harm_total; i++)
	{
		char ss[16];
		s_HarmonicWidgets *hw = harm_widgets + i;
		gpointer data = (void*)(intptr_t)(i + 1);

		if(i)
		{
			hw->separ = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
			gtk_container_add (GTK_CONTAINER (HarmBox), hw->separ);
		}


		hw->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
		gtk_widget_set_margin_start (hw->box, 5);
		gtk_widget_set_margin_end   (hw->box, 5);
		// метка с номером гармоники
		sprintf(ss, "%i", i + 1);
		hw->label = gtk_label_new(ss);
		gtk_container_add (GTK_CONTAINER (hw->box), hw->label);
		// амплитуда
		create_mag_phase_scale(&hw->mag, 0, data);
		add_mag_phase_scale_2_box(hw->box, &hw->mag);

		// фаза
		create_mag_phase_scale(&hw->phase, 1, data);
		add_mag_phase_scale_2_box(hw->box, &hw->phase);

		// кнопка фаза = 0
		hw->btn_phase0 = gtk_button_new();
		gtk_button_set_label(GTK_BUTTON(hw->btn_phase0), ">0<");
		g_signal_connect(hw->btn_phase0, "clicked", G_CALLBACK(on_btn_Phase0_clicked), data);
		gtk_container_add (GTK_CONTAINER (hw->box), hw->btn_phase0);

		// разрешение
		hw->chk_enabled = gtk_check_button_new();
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
		gtk_widget_set_halign (hw->chk_enabled, GTK_ALIGN_CENTER);
		gtk_container_add (GTK_CONTAINER (hw->box), hw->chk_enabled);
		g_signal_connect(hw->chk_enabled, "toggled", G_CALLBACK(on_MagPhase_changed), data);


		// добавляем к основному боксу
		gtk_container_add (GTK_CONTAINER (HarmBox), hw->box);
	}
}
//---------------------------------------------------------------------------------------
void set_harm_set_visible(s_HarmonicWidgets *harm, gboolean en)
{
//	gtk_widget_set_visible  (harm->label, en);
//	gtk_widget_set_visible  (harm->mag.label, en);
//	gtk_widget_set_visible  (harm->mag.scale, en);
//	gtk_widget_set_visible  (harm->phase.label, en);
//	gtk_widget_set_visible  (harm->phase.scale, en);
//	gtk_widget_set_visible  (harm->chk_enabled, en);
//	gtk_widget_set_visible  (harm->btn_phase0, en);
	gtk_widget_set_visible  (harm->box, en);
}
//---------------------------------------------------------------------------------------
void set_harm_set_sensitive(s_HarmonicWidgets *harm, gboolean en)
{
	gtk_widget_set_sensitive (harm->mag.scale, en);
	gtk_widget_set_sensitive (harm->phase.scale, en);
	gtk_widget_set_sensitive (harm->chk_enabled, en);
	gtk_widget_set_sensitive (harm->btn_phase0, en);
}
//---------------------------------------------------------------------------------------
void all_harms_set(void)
{
	int harm_total = NUMOFARRAY(harm_widgets);
	int harm_num = gtk_adjustment_get_value (adj_HarmNum);
	for(int i = 0; i < harm_total; i++)
	{
		s_HarmonicWidgets *hw = harm_widgets + i;
		set_harm_set_sensitive(hw, i < harm_num);
		//set_harm_set_visible(hw, i <= harm_num);
	}
}
//---------------------------------------------------------------------------------------
void preset_to_harmonics(int type)
{
	harmonics_in_update = TRUE;
	switch(type)
	{
		case 0:  // first harmonic
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				//int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				gtk_adjustment_set_value(hw->mag.adj, (i == 0) ? 1.0 : 0.0);
				gtk_adjustment_set_value(hw->phase.adj, 0);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_adjustment_set_value (adj_Amplify, 0);
			}
		break;

		case 1:  // meander
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				double mag = 0;
				if(h & 1) mag = 1.0 / h;
				gtk_adjustment_set_value(hw->mag.adj, mag);
				gtk_adjustment_set_value(hw->phase.adj, 0);
				//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), h & 1);
				gtk_adjustment_set_value (adj_Amplify, 0);
			}
		break;

		case 2:  // meander shifted
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				double mag = 0;
				if(h & 1) mag = 1.0 / h;
				gtk_adjustment_set_value(hw->mag.adj, mag);
				gtk_adjustment_set_value(hw->phase.adj, limit_degree(45 * h));
				//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), h & 1);
				gtk_adjustment_set_value (adj_Amplify, 0);

			}
		break;

		case 3:  // sawtooth
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				gtk_adjustment_set_value(hw->mag.adj, 1.0 / h);
				gtk_adjustment_set_value(hw->phase.adj, 0.0);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_adjustment_set_value (adj_Amplify, 0);

			}
		break;

		case 4:  // triangle
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				double mag = 0;
				if(h & 1) mag = 1.0 / h / h;
				gtk_adjustment_set_value(hw->mag.adj, mag);
				gtk_adjustment_set_value(hw->phase.adj, 90);
				//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), h & 1);
				gtk_adjustment_set_value (adj_Amplify, 0);
			}
		break;

		case 5:  // single pulse
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				double phase = 90;
				if(h & 1) phase = -90;
				s_HarmonicWidgets *hw = harm_widgets + i;
				gtk_adjustment_set_value(hw->mag.adj, 1);
				gtk_adjustment_set_value(hw->phase.adj, phase);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), TRUE);
				gtk_adjustment_set_value (adj_Amplify, -30);
			}
		break;

		case 6:  // dual peak
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				double mag = 0;
				if(h & 1) mag = 1;
				gtk_adjustment_set_value(hw->mag.adj, mag);
				gtk_adjustment_set_value(hw->phase.adj, 90);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), h & 1);
				gtk_adjustment_set_value (adj_Amplify, -24);
			}
		break;

		case 7:  // dual peak (45º)
			for(int i = 0; i < NUMOFARRAY(harm_widgets); i++)
			{
				int h = i + 1;
				s_HarmonicWidgets *hw = harm_widgets + i;
				double mag = 0;
				if(h & 1) mag = 1;
				gtk_adjustment_set_value(hw->mag.adj, mag);
				gtk_adjustment_set_value(hw->phase.adj, limit_degree(90 + 45 * h));
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->chk_enabled), h & 1);
				gtk_adjustment_set_value (adj_Amplify, -24);
			}
		break;


	}
	harmonics_in_update = FALSE;
	gtk_widget_queue_draw(drawing_area);
}
//---------------------------------------------------------------------------------------
void update_mag_phase_val(int idx)
{
	const int num = NUMOFARRAY(harm_widgets);
	if(idx < 0)
	{
		for(int i = 0; i < num; i++)
		{
			harm_widgets[i].mag.val   = gtk_adjustment_get_value (harm_widgets[i].mag.adj) * 0x10000LL;
			harm_widgets[i].phase.val = gtk_adjustment_get_value (harm_widgets[i].phase.adj) * 0x100000000LL / 360;
			harm_widgets[i].en        = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(harm_widgets[i].chk_enabled));
		}
	}
	else if(idx >= 0 && idx < num)
	{
		harm_widgets[idx].mag.val   = gtk_adjustment_get_value (harm_widgets[idx].mag.adj) * 0x10000LL;
		harm_widgets[idx].phase.val = gtk_adjustment_get_value (harm_widgets[idx].phase.adj) * 0x100000000LL / 360;
		harm_widgets[idx].en        = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(harm_widgets[idx].chk_enabled));
		//printf("Button %i active = %i\n", idx, harm_widgets[idx].en);
	}
}
//---------------------------------------------------------------------------------------
void redraw_signal(cairo_t *cr)
{
	// если идёт массовое обновление параметров гармоник - не рисуем. Нас вызовут позже
	if(harmonics_in_update) return;

	int w = gtk_widget_get_allocated_width(drawing_area);
	int h = gtk_widget_get_allocated_height(drawing_area);
	int hh = h / 2;



	//printf("redraw, w = %i, h = %i\n", w, h);
	//double comm_phase = gtk_adjustment_get_value(adj_CommonPhase) * G_PI / 180;

	double amp_db = gtk_adjustment_get_value (adj_Amplify);
	// scale X ~~ fix.point 0.32
	uint32_t sx = 1.25 * 0x100000000LL / w;
	// scale Y ~~ fix.point 16.16
	uint32_t sy = pow(10, amp_db / 20.0) * 0x10000;
	sy = sy * h / 4;

	// common phase - fix.point 0.32
	int32_t comm_phase = gtk_adjustment_get_value(adj_CommonPhase) * 0x100000000LL / 360;
	int32_t comm_DC    = gtk_adjustment_get_value(adj_CommonDC) * sy / 0x10000;
	//double sx = 2.5 * G_PI / w;
//	double sy = (double)h / 4.0 * pow(10, amp_db / 20.0);


	int    ovl = w / 10;            // сколько пикселов захватываем на соседних периодах
	int harm_num = gtk_adjustment_get_value (adj_HarmNum);

	// crosshair
	cairo_set_source_rgb(cr, 0.8, 1, 0.8);
	cairo_set_line_width(cr, 1);
	cairo_move_to(cr, 0,   hh);
	cairo_line_to(cr, w-1, hh);
	cairo_move_to(cr, w/2, 0);
	cairo_line_to(cr, w/2, h-1);
	cairo_move_to(cr, ovl, 0);
	cairo_line_to(cr, ovl, h-1);
	cairo_move_to(cr, w-ovl, 0);
	cairo_line_to(cr, w-ovl, h-1);
	cairo_stroke(cr);

	// signal
	cairo_set_source_rgb(cr, 0, 0, 0.5);
	cairo_set_line_width(cr, 1.5);

	//

	for(int x = 0; x < w; x++)
	{
		//double v = 0;
		int64_t v64 = 0;
		for(int i = 0; i < harm_num; i++)
		{
			int h = i + 1;
			if(harm_widgets[i].en)
			{
				uint32_t phase = sx * (x - ovl) * h;
				phase -= (uint64_t)comm_phase * h;
				phase += harm_widgets[i].phase.val;
				int64_t i64 = get_sine_int32(sine_approx_table_256_order_1_acs_5, 8, 1, 5, phase);
				v64 += i64 * harm_widgets[i].mag.val;
			}
		}

		v64 >>= 16;
		v64 *= sy;
		v64 >>= 32;

		int y = v64;
		double yy = hh - (double)y / 0x4000 - comm_DC;

		if(!x) cairo_move_to(cr, x, yy);
		else   cairo_line_to(cr, x, yy);
	}
	cairo_stroke(cr);
}
//---------------------------------------------------------------------------------------
//gboolean draw_circle (GtkWidget *widget, cairo_t *cr, gpointer data)
//{
//    guint width, height;
//    //GdkRGBA color;
//    //GtkStyleContext *context;
//
//    //context = gtk_widget_get_style_context (widget);
//    width = gtk_widget_get_allocated_width (widget);
//    height = gtk_widget_get_allocated_height (widget);
//    //gtk_render_background(context, cr, 0, 0, width, height);
//    cairo_arc (cr, width/2.0, height/2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);
//    //gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);
//    //gdk_cairo_set_source_rgba (cr, &color);
//    //gdk_cairo_set_source_rgba (cr, &color);
//    cairo_fill (cr);
//    return FALSE;
//}
//------------------------------------------------------------------------------------------------------
void create_draw_window(void)
{
	window_draw = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window_draw), "Draw results");
	//gtk_window_set_decorated (GTK_WINDOW(window_draw), FALSE);
	g_signal_connect(window_draw, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_window_set_default_size(GTK_WINDOW(window_draw), 400, 300);

	drawing_area = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER(window_draw), drawing_area);

	g_signal_connect (G_OBJECT (drawing_area), "draw", G_CALLBACK (draw_area_on_draw), NULL);
	//g_signal_connect (G_OBJECT (drawing_area), "draw", G_CALLBACK (draw_circle), NULL);

	//g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (helloWorld), (gpointer) win);
	//g_signal_connect(drawing_area, "draw", draw_area_on_draw, NULL);

}
//---------------------------------------------------------------------------------------
//G_MODULE_EXPORT void on_btn__clicked (GtkButton *button, gpointer user_data)
//{
//	printf("on_btn_OpenFile_clicked\n");
//}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_btn_Amplify0_clicked (GtkButton *button, gpointer user_data)
{
	//printf("on_btn_OpenFile_clicked\n");
	gtk_adjustment_set_value(adj_Amplify, 0.0);
}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_btn_CommPhase0_clicked (GtkButton *button, gpointer user_data)
{
	//printf("on_btn_CommPhase0_clicked\n");
	gtk_adjustment_set_value(adj_CommonPhase, 0.0);
}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_btn_CommDC0_clicked (GtkButton *button, gpointer user_data)
{
	//printf("on_btn_CommPhase0_clicked\n");
	gtk_adjustment_set_value(adj_CommonDC, 0.0);
}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_btn_Phase0_clicked (GtkButton *button, gpointer user_data)
{
	//printf("on_btn_Phase0_clicked, data = %p\n", data);
	if(user_data)
	{
		int idx = (intptr_t)user_data;
		--idx;
		if(idx >= 0 && idx < NUMOFARRAY(harm_widgets))
		{
			gtk_adjustment_set_value(harm_widgets[idx].phase.adj, 0.0);
		}
	}

}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT gboolean draw_area_on_draw (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	//printf("draw_area_on_draw\n");

	redraw_signal(cr);
	//draw_circle(widget, cr, data);

	return FALSE;
}

//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_adj_Common_value_changed (GtkAdjustment *adjustment, gpointer user_data)
{
	gtk_widget_queue_draw(drawing_area);
}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_MagPhase_changed (GtkAdjustment *adjustment, gpointer user_data)
{
	//printf("on_MagPhase_changed, data = %p\n", user_data);
	if(user_data)
	{
		int idx = (intptr_t)user_data;
		update_mag_phase_val(idx - 1);
	}
	gtk_widget_queue_draw(drawing_area);
}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_HarmNum_changed (GtkAdjustment *adjustment, gpointer user_data)
{
	//printf("sig_HarmNum_changed\n");
	all_harms_set();
	gtk_widget_queue_draw(drawing_area);
}
//---------------------------------------------------------------------------------------
//G_MODULE_EXPORT void on_adj_Amplify_value_changed (GtkAdjustment *adjustment, gpointer user_data)
//{
//	//printf("sig_HarmNum_changed\n");
//	gtk_widget_queue_draw(drawing_area);
//}
//---------------------------------------------------------------------------------------
G_MODULE_EXPORT void on_cbx_Preset_changed (GtkComboBox *combobox, gpointer user_data)
{
	//printf("on_cbx_Preset_changed\n");
	//all_harms_set();
	preset_to_harmonics(gtk_combo_box_get_active (combobox));
	gtk_widget_queue_draw(drawing_area);
}
//--------------------------------------------------------------------------------------
int main (int argc, char *argv[])
{
	/* запускаем GTK+ */
	gtk_init (&argc, &argv);

	/* вызываем нашу функцию для создания окна */
	create_main_window ();
	create_harmset();

	gtk_widget_show_all(window_main);
	all_harms_set();

	create_draw_window ();
	gtk_widget_show_all (window_draw);

	preset_to_harmonics(0);

	/* передаём управление GTK+ */
	gtk_main ();
	return 0;
}

