@tool
extends VBoxContainer
class_name MPaintPanel

@onready var brush_type_checkbox:=$brush_type
@onready var brush_list_option:=$brush_list

var float_prop_element=preload("res://addons/m_terrain/gui/control_prop_element/float.tscn")
var float_range_prop_element=preload("res://addons/m_terrain/gui/control_prop_element/float_range.tscn")

var brush_manager:MBrushManager = MBrushManager.new()
var is_color_brush:=true
var brush_id:int=-1

var property_element_list:Array

func _ready():
	_on_brush_type_toggled(false)

func _on_brush_type_toggled(button_pressed):
	is_color_brush = button_pressed
	brush_list_option.clear()
	if button_pressed:
		brush_type_checkbox.text = "Color brush"
		_on_brush_list_item_selected(-1)
	else:
		brush_type_checkbox.text = "Height brush"
		var brushe_names = brush_manager.get_height_brush_list()
		for n in brushe_names:
			brush_list_option.add_item(n)
			_on_brush_list_item_selected(0)


func _on_brush_list_item_selected(index):
	clear_property_element()
	if index < -1: return
	brush_id = index
	var brush_props:Array
	if is_color_brush:
		pass
	else:
		brush_props = brush_manager.get_height_brush_property(brush_id)
	for p in brush_props:
		create_props(p)



func create_props(dic:Dictionary):
	print(dic)
	var element
	if dic["type"]==TYPE_FLOAT:
		var rng = dic["max"] - dic["min"]
		if dic["hint"] == "range":
			element = float_range_prop_element.instantiate()
			element.set_name(dic["name"])
			element.set_value(dic["default_value"])
			element.set_min(dic["min"])
			element.set_max(dic["max"])
			element.set_step(dic["hint_string"].to_float())
			element.connect("prop_changed",Callable(self,"prop_change"))
			add_child(element)
		else:
			element = float_prop_element.instantiate()
			element.set_name(dic["name"])
			element.set_value(dic["default_value"])
			element.min = dic["min"]
			element.max = dic["max"]
			element.connect("prop_changed",Callable(self,"prop_change"))
			add_child(element)
	property_element_list.append(element)



func clear_property_element():
	print("free ", property_element_list)
	for e in property_element_list:
		e.queue_free()
	property_element_list = []

func prop_change(prop_name,value):
	if is_color_brush:
		pass
	else:
		brush_manager.set_height_brush_property(prop_name,value,brush_id)
