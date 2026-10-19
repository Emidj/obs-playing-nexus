#include "playing/ui/i18n.hpp"
// SPDX-License-Identifier: MIT
#include "preset-manager.hpp"
#include "playing/visual/normal-native-layout.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QPointer>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace playing::presets {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
QString nameChecked(QString name){name=name.trimmed();require(!name.isEmpty()&&name.size()<=80,playing::ui::text("El nombre debe tener entre 1 y 80 caracteres."));for(const auto c:name)require(!c.isNull()&&c.category()!=QChar::Other_Control&&c.category()!=QChar::Other_Format,playing::ui::text("El nombre contiene caracteres de control."));return name;}
QJsonObject selection(int normal,int pro){return {{"normal_preset",normal},{"pro_preset",pro},{"preset_family",pro?1:0},{"preset_style",pro?pro:normal}};}
QString baseOf(const QJsonObject& value){const int normal=value.value("normal_preset").toInt(),pro=value.value("pro_preset").toInt();for(const auto& e:builtins())if(e.appearance.value("normal_preset").toInt()==normal&&e.appearance.value("pro_preset").toInt()==pro)return e.id;for(const auto& alias:visual::normal::normalAliases())if(alias.setting==normal)for(const auto& e:visual::normal::normalCatalogue())if(e.preset==alias.preset)return "normal:"+QString::fromStdString(e.id);throw std::runtime_error(playing::ui::text("Estilo base desconocido."));}
QJsonObject emptyDocument(){return {{"version",1},{"presets",QJsonArray{}},{"favorites",QJsonArray{}}};}
QVector<Entry> decode(const QJsonObject& document){
    require(document.size()==3&&document.value("version")==1&&document.value("presets").isArray()&&document.value("favorites").isArray(),playing::ui::text("Biblioteca incompatible o dañada; no se modificará."));
    QVector<Entry> result=builtins();QStringList ids;for(const auto& e:result)ids.append(e.id);
    const auto presets=document.value("presets").toArray();require(presets.size()<=128,playing::ui::text("Demasiados presets guardados."));
    for(const auto& item:presets){require(item.isObject(),playing::ui::text("Preset dañado."));const auto o=item.toObject();require(o.size()==5&&o.value("id").isString()&&o.value("name").isString()&&o.value("base").isString()&&o.value("appearance").isObject(),playing::ui::text("Preset dañado."));
        Entry e{o.value("id").toString(),nameChecked(o.value("name").toString()),o.value("base").toString(),o.value("group").toInt(-1),o.value("appearance").toObject()};
        require(e.id.startsWith("user:")&&!QUuid(e.id.mid(5)).isNull()&&!ids.contains(e.id)&&(e.group==2||e.group==3),playing::ui::text("Identificador de preset inválido."));validate(e.appearance);require(e.base==baseOf(e.appearance),playing::ui::text("Base de preset inválida."));ids.append(e.id);result.append(e);
    }
    const auto favorites=document.value("favorites").toArray();require(favorites.size()<=256,playing::ui::text("Demasiados favoritos."));QStringList seen;
    for(const auto& id:favorites){require(id.isString()&&ids.contains(id.toString())&&!seen.contains(id.toString()),playing::ui::text("Favorito inválido."));seen.append(id.toString());}return result;
}
QJsonObject read(const QString& path){QFile file(path);if(!file.exists())return emptyDocument();require(file.open(QIODevice::ReadOnly),playing::ui::text("No se pudo leer la biblioteca."));require(file.size()<=512*1024,playing::ui::text("Biblioteca demasiado grande."));const auto bytes=file.read(512*1024+1);require(bytes.size()<=512*1024&&file.atEnd(),playing::ui::text("Biblioteca demasiado grande."));QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);require(error.error==QJsonParseError::NoError&&doc.isObject(),playing::ui::text("Biblioteca dañada; no se modificará."));decode(doc.object());return doc.object();}
}
const QVector<Entry>& builtins(){static const auto values=[] {
    const auto defaults=[](int normal,int pro){auto value=selection(normal,pro);value["artwork_enabled"]=pro==1||pro>=4;return value;};
    QVector<Entry> list;list.append({"basic","Texto manual",{},0,defaults(0,0)});
    for(const auto& e:visual::normal::normalCatalogue())list.append({"normal:"+QString::fromStdString(e.id),QString::fromStdString(e.name),{},0,defaults(e.setting,0)});
    const char* names[]={"Neon Club","Solo texto horizontal","Solo texto vertical","Vinyl Classic","Minimal","Glassmorph","Urban","Waveform","HUD","Particles","Liquid","Vertical Retro","Vertical Glass","Vertical Vinyl","Vertical Urban","Vertical Liquid"};
    for(int i=0;i<16;++i)list.append({"pro:"+QString::number(i+1),QString::fromUtf8(names[i]),{},1,defaults(0,i+1)});return list;
}();return values;}
QStringList visualKeys(){return {"normal_preset","pro_preset","preset_family","preset_style","canvas_width","canvas_height","text_layout","text_offset_x","text_offset_y","text_title_font","text_artist_font","text_title_color","text_artist_color","text_margin","text_line_gap","text_top_distance","text_image_gap","text_cover_size","artwork_enabled","artwork_x","artwork_y","artwork_width","artwork_height","title_x","title_y","title_width","title_height","artist_x","artist_y","artist_width","artist_height","title_align","artist_align","title_overflow","artist_overflow","title_minimum","artist_minimum","normal_transition","normal_duration","normal_gap","normal_speed","normal_pause","transition_whole","pro_hue","pro_light_intensity","pro_light_pulse","pro_panel_opacity","animation_speed","title_outline_width","title_outline_color","title_shadow_offset","title_shadow_color","artist_outline_width","artist_outline_color","artist_shadow_offset","artist_shadow_color"};}
void validate(const QJsonObject& appearance){
    const auto keys=visualKeys();require(appearance.size()<=keys.size(),playing::ui::text("Demasiados ajustes."));
    for(auto it=appearance.begin();it!=appearance.end();++it){const auto key=it.key();require(keys.contains(key),playing::ui::text("El preset contiene un ajuste no visual."));
        if(key.endsWith("_font")){require(it->isObject(),playing::ui::text("Tipografía inválida."));const auto f=it->toObject();require(f.size()<=4,playing::ui::text("Tipografía inválida."));for(auto field=f.begin();field!=f.end();++field){if(field.key()=="face"||field.key()=="style"){require(field->isString()&&field->toString().size()<=128,playing::ui::text("Tipografía inválida."));for(const auto c:field->toString())require(c.category()!=QChar::Other_Control&&c.category()!=QChar::Other_Format,playing::ui::text("Tipografía inválida."));}else if(field.key()=="size"||field.key()=="flags"){const double n=field->toDouble(-1);require(field->isDouble()&&std::isfinite(n)&&std::floor(n)==n&&n>=0&&n<=(field.key()=="size"?512:15),playing::ui::text("Tipografía inválida."));}else require(false,playing::ui::text("Campo de tipografía desconocido."));}continue;}
        if(key=="artwork_enabled"||key=="transition_whole"||key=="pro_light_pulse"){require(it->isBool(),playing::ui::text("Ajuste booleano inválido."));continue;}
        const double n=it->toDouble(-9999);double low=0,high=2048;
        if(key.endsWith("_outline_width")||key.endsWith("_shadow_offset"))high=8;else if(key.endsWith("_color"))high=4294967295.0;else if(key=="text_line_gap")low=-128,high=128;else if(key=="text_offset_x"||key=="text_offset_y")low=-2048,high=2048;
        else if(key=="normal_duration")low=200,high=2000;else if(key=="normal_speed")low=10,high=150;
        else if(key=="normal_pause")high=10;else if(key=="normal_gap")high=80;
        else if(key=="animation_speed")low=25,high=300;else if(key=="pro_hue")high=360;
        else if(key=="pro_light_intensity"||key=="pro_panel_opacity")high=100;
        else if(key=="canvas_width"||key=="canvas_height")low=64;
        else if(key.endsWith("_overflow"))high=4;else if(key.endsWith("_align")||key=="normal_transition"||key=="text_layout")high=2;
        else if(key=="preset_family")high=1;else if(key=="pro_preset")high=16;
        require(it->isDouble()&&std::isfinite(n)&&std::floor(n)==n&&n>=low&&n<=high,playing::ui::text("Ajuste visual fuera de rango."));
    }
    require(appearance.contains("normal_preset")&&appearance.contains("pro_preset")&&!(appearance.value("normal_preset").toInt()&&appearance.value("pro_preset").toInt()),playing::ui::text("Selección de estilo inválida."));baseOf(appearance);
    const int pro=appearance.value("pro_preset").toInt();require(appearance.value("preset_family").toInt()==(pro?1:0)&&appearance.value("preset_style").toInt()==appearance.value(pro?"pro_preset":"normal_preset").toInt(),playing::ui::text("Selectores de estilo incompatibles."));
}
QJsonObject capture(const QJsonObject& settings){QJsonObject result;for(const auto& key:visualKeys())if(settings.contains(key))result.insert(key,settings.value(key));const int normal=result.value("normal_preset").toInt(),pro=result.value("pro_preset").toInt();const auto selected=selection(normal,pro);for(auto it=selected.begin();it!=selected.end();++it)result.insert(it.key(),it.value());validate(result);return result;}
QByteArray encodePreset(const Entry& entry){
    validate(entry.appearance);const auto name=nameChecked(entry.name),base=baseOf(entry.appearance);
    const auto bytes=QJsonDocument(QJsonObject{{"format","obs-playing-preset"},{"version",1},{"name",name},{"base",base},{"appearance",entry.appearance}}).toJson();
    require(bytes.size()<=64*1024,playing::ui::text("El archivo de preset es demasiado grande."));return bytes;
}
Entry decodePreset(const QByteArray& bytes){
    require(!bytes.isEmpty()&&bytes.size()<=64*1024,playing::ui::text("Archivo de preset vacío o demasiado grande."));
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);require(error.error==QJsonParseError::NoError&&document.isObject(),playing::ui::text("Archivo de preset dañado."));
    const auto value=document.object();require(value.size()==5&&value.value("format")=="obs-playing-preset"&&value.value("version")==1&&value.value("name").isString()&&value.value("base").isString()&&value.value("appearance").isObject(),playing::ui::text("Formato o versión de preset no compatible."));
    Entry entry{{},nameChecked(value.value("name").toString()),value.value("base").toString(),3,value.value("appearance").toObject()};validate(entry.appearance);require(entry.base==baseOf(entry.appearance),playing::ui::text("Estilo base incompatible con los ajustes."));return entry;
}
namespace {
void presetPath(const QString& path){const QFileInfo file(path);require(file.isAbsolute()&&file.suffix().compare("obsplayingpreset",Qt::CaseInsensitive)==0,playing::ui::text("Selecciona un archivo .obsplayingpreset con ruta absoluta."));}
}
Library::Library(QString path):path_(std::move(path)){reload();}
void Library::reload(){document_=read(path_);}
QVector<Entry> Library::entries()const{return decode(document_);}
bool Library::favorite(const QString& id)const{return document_.value("favorites").toArray().contains(id);}
void Library::mutate(const std::function<void(QJsonObject&)>& operation){
    require(QDir().mkpath(QFileInfo(path_).absolutePath()),playing::ui::text("No se pudo crear la biblioteca."));QLockFile lock(path_+".lock");require(lock.tryLock(0),playing::ui::text("La biblioteca está abierta por otra operación. Inténtalo de nuevo."));auto next=read(path_);operation(next);decode(next);const auto bytes=QJsonDocument(next).toJson();require(bytes.size()<=512*1024,playing::ui::text("Biblioteca demasiado grande."));QSaveFile file(path_);file.setDirectWriteFallback(false);require(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit(),playing::ui::text("No se pudo guardar la biblioteca."));document_=next;
}
void Library::toggleFavorite(const QString& id){mutate([&](auto& d){bool exists=false;for(const auto& e:decode(d))exists|=e.id==id;require(exists,playing::ui::text("Preset no encontrado."));auto f=d.value("favorites").toArray();const auto index=f.contains(id)?[&]{for(int i=0;i<f.size();++i)if(f[i]==id)return i;return -1;}():-1;if(index>=0)f.removeAt(index);else f.append(id);d["favorites"]=f;});}
QString Library::saveAs(const QString& name,const QJsonObject& appearance){validate(appearance);const auto title=nameChecked(name),id="user:"+QUuid::createUuid().toString(QUuid::WithoutBraces);mutate([&](auto& d){auto items=d.value("presets").toArray();items.append(QJsonObject{{"id",id},{"name",title},{"base",baseOf(appearance)},{"group",2},{"appearance",appearance}});d["presets"]=items;});return id;}
void Library::replace(const QString& id,const QJsonObject& appearance){validate(appearance);mutate([&](auto& d){auto a=d.value("presets").toArray();bool found=false;for(int i=0;i<a.size();++i)if(a[i].toObject().value("id")==id){auto o=a[i].toObject();o["appearance"]=appearance;o["base"]=baseOf(appearance);a[i]=o;found=true;}require(found,playing::ui::text("Solo se pueden actualizar presets guardados."));d["presets"]=a;});}
void Library::rename(const QString& id,const QString& name){const auto title=nameChecked(name);mutate([&](auto& d){auto a=d.value("presets").toArray();bool found=false;for(int i=0;i<a.size();++i)if(a[i].toObject().value("id")==id){auto o=a[i].toObject();o["name"]=title;a[i]=o;found=true;}require(found,playing::ui::text("Solo se pueden renombrar presets guardados."));d["presets"]=a;});}
void Library::remove(const QString& id){mutate([&](auto& d){auto a=d.value("presets").toArray();bool found=false;for(int i=a.size()-1;i>=0;--i)if(a[i].toObject().value("id")==id){a.removeAt(i);found=true;}require(found,playing::ui::text("Los estilos originales no se pueden eliminar."));auto f=d.value("favorites").toArray();for(int i=f.size()-1;i>=0;--i)if(f[i]==id)f.removeAt(i);d["presets"]=a;d["favorites"]=f;});}
QString Library::importFile(const QString& path){
    presetPath(path);QFile file(path);require(file.open(QIODevice::ReadOnly)&&file.size()<=64*1024,playing::ui::text("No se pudo leer el preset o supera 64 KiB."));const auto bytes=file.read(64*1024+1);require(file.atEnd()&&bytes.size()<=64*1024,playing::ui::text("Archivo de preset demasiado grande."));const auto entry=decodePreset(bytes);QString id;
    mutate([&](auto& document){for(const auto& existing:decode(document))if(existing.group==3&&existing.name==entry.name&&existing.appearance==entry.appearance){id=existing.id;return;}id="user:"+QUuid::createUuid().toString(QUuid::WithoutBraces);auto values=document.value("presets").toArray();values.append(QJsonObject{{"id",id},{"name",entry.name},{"base",entry.base},{"group",3},{"appearance",entry.appearance}});document["presets"]=values;});return id;
}
void Library::exportFile(const QString& id,const QString& path){
    presetPath(path);reload();Entry entry;bool found=false;for(const auto& item:entries())if(item.id==id){entry=item;found=true;break;}require(found,playing::ui::text("Preset no encontrado."));const auto bytes=encodePreset(entry);
    QSaveFile file(path);file.setDirectWriteFallback(false);require(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit(),playing::ui::text("No se pudo exportar el preset."));
}
QDialog* showManager(const QString& path,std::function<QJsonObject()> current,std::function<void(const Entry&)> apply,QWidget* parent){
    auto library=std::make_shared<Library>(path);auto owned=std::make_unique<QDialog>(parent);auto dialog=owned.get();dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(playing::ui::text("OBS Playing — Gestor de presets"));dialog->resize(600,540);
    auto layout=new QVBoxLayout(dialog);auto category=new QComboBox(dialog);category->setObjectName("presetCategory");category->addItems({playing::ui::text("Clásicos"),"PRO",playing::ui::text("Mis presets"),playing::ui::text("Importados"),playing::ui::text("Favoritos")});layout->addWidget(category);
    auto search=new QLineEdit(dialog);search->setObjectName("presetSearch");search->setPlaceholderText(playing::ui::text("Buscar estilo o preset…"));layout->addWidget(search);auto list=new QListWidget(dialog);list->setObjectName("presetList");layout->addWidget(list);
    auto status=new QLabel(dialog);status->setWordWrap(true);layout->addWidget(status);auto hint=new QLabel(playing::ui::text("Solo cambia el diseño. El programa DJ, las imágenes locales y la conexión se conservan."),dialog);hint->setWordWrap(true);layout->addWidget(hint);
    auto row=new QHBoxLayout;layout->addLayout(row);auto action=[&](const char* text,const char* id){auto button=new QPushButton(QString::fromUtf8(text),dialog);button->setObjectName(id);row->addWidget(button);return button;};
    auto use=action(playing::ui::text("Aplicar"),"presetApply"),fav=action(playing::ui::text("Favorito"),"presetFavorite"),save=action(playing::ui::text("Guardar como…"),"presetSaveAs");
    row=new QHBoxLayout;layout->addLayout(row);auto import=action(playing::ui::text("Importar…"),"presetImport"),exportPreset=action(playing::ui::text("Exportar seleccionado…"),"presetExport");
    row=new QHBoxLayout;layout->addLayout(row);auto update=action(playing::ui::text("Actualizar"),"presetUpdate"),rename=action(playing::ui::text("Renombrar"),"presetRename"),remove=action(playing::ui::text("Eliminar"),"presetDelete"),reset=action(playing::ui::text("Restablecer base"),"presetReset"),close=action(playing::ui::text("Cerrar"),"presetClose");
    auto selected=[=](){const auto item=list->currentItem();require(item!=nullptr,playing::ui::text("Selecciona un preset."));const auto id=item->data(Qt::UserRole).toString();for(const auto& e:library->entries())if(e.id==id)return e;throw std::runtime_error(playing::ui::text("Preset no encontrado."));};
    auto refresh=[=](QString id=QString{}){library->reload();if(id.isEmpty()&&list->currentItem())id=list->currentItem()->data(Qt::UserRole).toString();list->clear();for(const auto& e:library->entries())if((category->currentIndex()==4?library->favorite(e.id):e.group==category->currentIndex())&&(e.group<2?playing::ui::string(e.name.toUtf8().constData()):e.name).contains(search->text(),Qt::CaseInsensitive)){auto item=new QListWidgetItem((library->favorite(e.id)?QString::fromUtf8("★ "):QString{})+(e.group<2?playing::ui::string(e.name.toUtf8().constData()):e.name),list);item->setData(Qt::UserRole,e.id);if(e.id==id)list->setCurrentItem(item);}if(!list->currentItem()&&list->count())list->setCurrentRow(0);status->setText(list->count()?QString{}:QString::fromUtf8(playing::ui::text("No hay presets en esta categoría.")));};
    auto run=[=](std::function<void()> operation){try{operation();}catch(const std::exception& e){status->setText(QString::fromUtf8(e.what()));}};
    QObject::connect(category,&QComboBox::currentIndexChanged,dialog,[=]{run([=]{refresh();});});QObject::connect(search,&QLineEdit::textChanged,dialog,[=]{run([=]{refresh();});});
    QObject::connect(list,&QListWidget::currentItemChanged,dialog,[=]{const auto item=list->currentItem();const bool custom=item&&item->data(Qt::UserRole).toString().startsWith("user:");use->setEnabled(item);fav->setEnabled(item);exportPreset->setEnabled(item);reset->setEnabled(item);update->setEnabled(custom);rename->setEnabled(custom);remove->setEnabled(custom);});
    QObject::connect(import,&QPushButton::clicked,dialog,[=]{run([=]{const auto path=QFileDialog::getOpenFileName(dialog,playing::ui::text("Importar preset"),{},playing::ui::text("OBS Playing preset (*.obsplayingpreset)"));if(path.isEmpty())return;const auto id=library->importFile(path);search->clear();category->setCurrentIndex(3);refresh(id);status->setText(playing::ui::text("Preset importado. Selecciona Aplicar para usarlo."));});});
    QObject::connect(exportPreset,&QPushButton::clicked,dialog,[=]{run([=]{const auto entry=selected();const auto path=QFileDialog::getSaveFileName(dialog,playing::ui::text("Exportar preset"),"OBS-Playing-preset.obsplayingpreset",playing::ui::text("OBS Playing preset (*.obsplayingpreset)"));if(path.isEmpty())return;library->exportFile(entry.id,path);status->setText(playing::ui::text("Preset exportado."));});});
    QObject::connect(use,&QPushButton::clicked,dialog,[=]{run([=]{apply(selected());status->setText(playing::ui::text("Preset aplicado."));});});QObject::connect(fav,&QPushButton::clicked,dialog,[=]{run([=]{const auto e=selected();library->toggleFavorite(e.id);refresh(e.id);});});
    QObject::connect(save,&QPushButton::clicked,dialog,[=]{run([=]{bool ok=false;const auto name=QInputDialog::getText(dialog,playing::ui::text("Guardar preset"),playing::ui::text("Nombre"),QLineEdit::Normal,{},&ok);if(!ok)return;const auto id=library->saveAs(name,capture(current()));category->setCurrentIndex(2);refresh(id);status->setText(playing::ui::text("Diseño guardado."));});});
    QObject::connect(update,&QPushButton::clicked,dialog,[=]{run([=]{const auto e=selected();library->replace(e.id,capture(current()));refresh(e.id);status->setText(playing::ui::text("Ajustes actuales guardados."));});});
    QObject::connect(rename,&QPushButton::clicked,dialog,[=]{run([=]{const auto e=selected();bool ok=false;const auto name=QInputDialog::getText(dialog,playing::ui::text("Renombrar preset"),playing::ui::text("Nombre"),QLineEdit::Normal,e.name,&ok);if(ok){library->rename(e.id,name);refresh(e.id);}});});
    QObject::connect(remove,&QPushButton::clicked,dialog,[=]{run([=]{const auto e=selected();QPointer<QDialog> owner=dialog;QPointer<QMessageBox> confirmation=new QMessageBox(QMessageBox::Question,playing::ui::text("Eliminar preset"),playing::ui::text("¿Eliminar «")+e.name+playing::ui::text("» de la biblioteca?"),QMessageBox::Yes|QMessageBox::No,dialog);confirmation->setTextFormat(Qt::PlainText);confirmation->setDefaultButton(QMessageBox::No);const auto answer=confirmation->exec();if(confirmation)delete confirmation.data();if(owner&&answer==QMessageBox::Yes){library->remove(e.id);refresh();}});});
    QObject::connect(reset,&QPushButton::clicked,dialog,[=]{run([=]{const auto e=selected();for(const auto& base:builtins())if(base.id==(e.base.isEmpty()?e.id:e.base)){apply(base);status->setText(playing::ui::text("Diseño base restablecido; el preset guardado se conserva."));return;}});});QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::close);
    refresh();dialog->show();return owned.release();
}
}

