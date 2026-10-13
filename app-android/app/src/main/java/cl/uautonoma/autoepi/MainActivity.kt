package cl.uautonoma.autoepi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.List
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.lifecycle.viewmodel.compose.viewModel
import cl.uautonoma.autoepi.ui.CorridasScreen
import cl.uautonoma.autoepi.ui.ManualScreen
import cl.uautonoma.autoepi.ui.PanelScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { AutoEpiApp() }
    }
}

val Tinta = Color(0xFF14213D)
val Azul = Color(0xFF1F5FAD)
val Ambar = Color(0xFFF2B705)
val Papel = Color(0xFFF7F8F5)

@Composable
fun AutoEpiApp(vm: AutoViewModel = viewModel()) {
    val ui by vm.ui.collectAsState()
    var tab by rememberSaveable { mutableIntStateOf(0) }
    val snack = remember { SnackbarHostState() }

    LaunchedEffect(ui.mensaje) {
        if (ui.mensaje.isNotEmpty()) {
            snack.showSnackbar(ui.mensaje)
            vm.limpiarMensaje()
        }
    }

    MaterialTheme(
        colorScheme = lightColorScheme(
            primary = Tinta, secondary = Azul, tertiary = Ambar,
            background = Papel, surface = Color.White
        )
    ) {
        Scaffold(
            snackbarHost = { SnackbarHost(snack) },
            bottomBar = {
                NavigationBar {
                    NavigationBarItem(selected = tab == 0, onClick = { tab = 0 },
                        icon = { Icon(Icons.Filled.Home, null) }, label = { Text("Panel") })
                    NavigationBarItem(selected = tab == 1, onClick = { tab = 1 },
                        icon = { Icon(Icons.Filled.Build, null) }, label = { Text("Manual") })
                    NavigationBarItem(selected = tab == 2, onClick = { tab = 2 },
                        icon = { Icon(Icons.Filled.List, null) }, label = { Text("Corridas") })
                }
            }
        ) { pad ->
            Box(Modifier.fillMaxSize().padding(pad)) {
                when (tab) {
                    0 -> PanelScreen(ui, vm)
                    1 -> ManualScreen(ui, vm)
                    else -> CorridasScreen(ui, vm)
                }
            }
        }
    }
}